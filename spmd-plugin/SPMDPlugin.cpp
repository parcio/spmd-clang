/*
 *  SPMDClang - Clang plugin for correctness checking of MPI programs
 *  Copyright (C) 2026-2026  Michael Blesel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CommunicationGraph.h"
#include "SPMDData.h"
#include "utils.h"
#include "debug.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <llvm/Support/WithColor.h>
#include "llvm/Support/TimeProfiler.h"

using namespace clang;

namespace
{

// Global analytical data about spmd concepts used in the code
static SPMDData SPMD_DATA;

// Debug variable for CFG creation
static int FILTERED_CFGS = 0;

/// Matches all variable declarations that hae been annotated as a SPMD variable by the user
/// VarDecls are categorized in SPMD_DATA as Rank and Size vars
/// Matcher: varDecl(hasAttr(attr::SPMDHint)).bind("attr")
// class SPMDAttrCallback : public ast_matchers::MatchFinder::MatchCallback
// {
//   public:
//     SPMDAttrCallback() {}
//
//     void run(const ast_matchers::MatchFinder::MatchResult &Result)
//     {
//         const auto var_decl = Result.Nodes.getNodeAs<VarDecl>("attr");
//
//         for(auto A : var_decl->getAttrs()) {
//             if(auto spmd = llvm::dyn_cast<SPMDHintAttr>(A)) {
//               llvm::errs() << "FOUND SPMDHintAttr\n";
//               llvm::errs() << "Spelling: " << spmd->getSpelling() << "\n";
//               llvm::errs() << "Kind: " << spmd->getKind() << "\n";
//               llvm::errs() << spmd->getValueString(Result.Context->getPrintingPolicy()) << "\n";
//
//               StringRef arg = "";
//               switch(spmd->getOption()) {
//                   case SPMDHintAttr::Rank:
//                       arg = "rank";
//                       SPMD_DATA.add_rank_var(var_decl);
//                       break;
//                   case SPMDHintAttr::Size:
//                       arg = "size";
//                       SPMD_DATA.add_size_var(var_decl);
//                       break;
//                   default:
//                       error_print("Unknown argument for SPMDHintAttr");
//                 }
//                 llvm::errs() << "Option: " << arg << "\n";
//             }
//         }
//     }
// };


/// Creates a CFG for each function and adds them to SPMD_DATA
class CFGCallback : public ast_matchers::MatchFinder::MatchCallback 
{
  public:
    CFGCallback() {}

    void run(const ast_matchers::MatchFinder::MatchResult &Result)
    {
        const bool DEBUG = HANDLE_TRANSLATION_UNIT_DEBUG;
        const auto func = Result.Nodes.getNodeAs<FunctionDecl>("fn");
        //TODO WIP
        auto build_options = CFG::BuildOptions();
        build_options.AddImplicitDtors=true;
        build_options.AddTemporaryDtors=true;
        // \WIP
        if(auto cfg = CFG::buildCFG(func, func->getBody(), Result.Context, build_options)) {
            auto filename = Result.Context->getSourceManager().getFilename(Result.Context->getSourceManager().getFileLoc(func->getBeginLoc()));
            if(!Result.Context->getSourceManager().isInSystemHeader(func->getBeginLoc()) &&
                    !filename.ends_with("/mpicxx.h") &&
                    !filename.ends_with("/mpi.h") &&
                    !filename.ends_with("/omp.h")) {
                if(DEBUG) {
                    llvm::errs() << "CFG: " << func->getNameAsString() << "; " << filename << "\n";
                    cfg->print(llvm::errs(), Result.Context->getLangOpts(), false);
                }
                SPMD_DATA.add_cfg(std::move(cfg), func);
            } else {
                FILTERED_CFGS++;
            } 
        }
    }
};


/// Matches all MPI function calls and adds them to SPMD_Data
/// Matcher: callExpr(callee(functionDecl(namedDecl(matchesName("MPI_"))).bind("func"))).bind("call");
class MPICallback : public ast_matchers::MatchFinder::MatchCallback 
{
  public:
    MPICallback() {}

    void run(const ast_matchers::MatchFinder::MatchResult &Result)
    {
        const auto f = Result.Nodes.getNodeAs<FunctionDecl>("func");
        const auto call = Result.Nodes.getNodeAs<CallExpr>("call");
        
        auto filename = Result.Context->getSourceManager().getFilename(Result.Context->getSourceManager().getFileLoc(call->getBeginLoc()));
        // Filtering out calls from the C++ MPI header to only analyze the contents of the program itself
        if(!filename.ends_with("/mpicxx.h")) {
            SPMD_DATA.add_mpi_call(call, f);
        }

    }
};


/// Matches all use of SPMD variables as declRefExprs
/// Tries to analyze in what context the variable is used and add the use case to SPMD_DATA
/// Matcher: declRefExpr(to(varDecl(hasAttr(attr::SPMDHint)).bind("decl"))).bind("ref");
class SPMDVarCallback : public ast_matchers::MatchFinder::MatchCallback 
{
  public:
    SPMDVarCallback() {}

    enum UseCategory
    {
        Uncategorized,
        CallExpr,
    };

    struct SPMDVarUse
    {
        const DeclRefExpr *decl_ref;
        const VarDecl *var_decl;
        const DynTypedNode parent_node;
        UseCategory use_category;
    };

    void run(const ast_matchers::MatchFinder::MatchResult &Result)
    {
        const auto r = Result.Nodes.getNodeAs<DeclRefExpr>("ref");
        const auto d = Result.Nodes.getNodeAs<VarDecl>("decl");

        auto parents = Result.Context->getParentMapContext().getParents(*r);
        while(parents.size() > 0) {
            auto grand_parents = Result.Context->getParentMapContext().getParents(parents[0]);
            if(grand_parents.size() > 0 && (grand_parents[0].getNodeKind().asStringRef() == "CompoundStmt")) {
                if(parents[0].get<clang::CallExpr>() != nullptr) {
                    spmd_var_uses.push_back({r, d, parents[0], UseCategory::CallExpr});
                } else {
                    spmd_var_uses.push_back({r, d, parents[0], UseCategory::Uncategorized});
                }
                break;
            } 
            parents = grand_parents;
        }
    }

    std::vector<SPMDVarCallback::SPMDVarUse> spmd_var_uses;
};


/// Defines all actions of the Plugin
class SPMDConsumer : public ASTConsumer
{
  public:
    explicit SPMDConsumer(ASTContext *Context)
    {
        // const auto spmd_attr_node = ast_matchers::varDecl(ast_matchers::hasAttr(attr::SPMDHint)).bind("attr");
        // _match_finder.addMatcher(spmd_attr_node, &_spmd_handler);

        // const auto matching_node = ast_matchers::functionDecl(ast_matchers::isExpansionInMainFile()).bind("fn");
        const auto matching_node = ast_matchers::functionDecl(ast_matchers::isDefinition()).bind("fn");
        _match_finder.addMatcher(matching_node, &_cfg_handler);

        const auto mpi_node = ast_matchers::callExpr(ast_matchers::callee(ast_matchers::functionDecl(ast_matchers::namedDecl(ast_matchers::matchesName("MPI_"))).bind("func"))).bind("call");
        _match_finder.addMatcher(mpi_node, &_mpi_handler);

        // const auto spmd_var = ast_matchers::declRefExpr(ast_matchers::to(ast_matchers::varDecl(ast_matchers::hasAttr(attr::SPMDHint)).bind("decl"))).bind("ref");
        // _match_finder.addMatcher(spmd_var, &_spmd_var_handler);
    }

    /// Matches the uses of SPMD variables to MPI calls
    /// recognizes the initialization of NOT annotated MPI rank/size variables
    /// recognizes uninitialized SPMD variables
    void match_spmd_vars(ASTContext &Context)
    {
        for(auto var_use: _spmd_var_handler.spmd_var_uses) {
            const FunctionDecl *func_decl = nullptr;
            switch(var_use.use_category) {
                case SPMDVarCallback::UseCategory::CallExpr:
                    func_decl = var_use.parent_node.get<clang::CallExpr>()->getDirectCallee();
                    if(func_decl->getNameAsString() == "MPI_Comm_rank") {
                        llvm::errs() << "=====================================================\n";
                        llvm::errs() << "SPMD variable used in MPI_Comm_rank:\n";
                        // TODO util function for these outputs?
                        auto &D = Context.getDiagnostics();
                        if(SPMD_DATA.add_rank_var_init(var_use.var_decl, var_use.parent_node.get<clang::CallExpr>())) {
                            D.Report(var_use.var_decl->getLocation(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Rank variable"));
                            D.Report(var_use.parent_node.get<clang::CallExpr>()->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Function call"));
                        } else {
                            llvm::errs() << "MPI_Comm_rank call on an unknown rank variable\n";
                            var_use.parent_node.get<clang::CallExpr>()->dump();
                            D.Report(var_use.parent_node.get<clang::CallExpr>()->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Function call"));
                        }
                    } else if (func_decl->getNameAsString() == "MPI_Comm_size") {
                        llvm::errs() << "=====================================================\n";
                        llvm::errs() << "SPMD variable used in MPI_Comm_size:\n";
                        auto &D = Context.getDiagnostics();
                        if(SPMD_DATA.add_size_var_init(var_use.var_decl, var_use.parent_node.get<clang::CallExpr>())) {
                            D.Report(var_use.var_decl->getLocation(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Size variable"));
                            D.Report(var_use.parent_node.get<clang::CallExpr>()->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Function call"));
                        } else {
                            llvm::errs() << "MPI_Comm_size call on an unknown rank variable\n";
                            var_use.parent_node.get<clang::CallExpr>()->dump();
                            D.Report(var_use.parent_node.get<clang::CallExpr>()->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Function call"));
                        }
                    } else {
                        llvm::errs() << "=====================================================\n";
                        llvm::errs() << "SPMD variable used in uncategorized CallExpr:\n";
                        // var_use.parent_node.dump(llvm::errs(), Context);
                        auto &D = Context.getDiagnostics();
                        D.Report(var_use.var_decl->getLocation(), D.getCustomDiagID(DiagnosticsEngine::Remark, "SPMD variable"));
                        D.Report(var_use.parent_node.get<clang::CallExpr>()->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Remark, "Function call"));
                    }
                    break;
                case SPMDVarCallback::UseCategory::Uncategorized:
                    break;
            }
        }
        // auto unmatched_rank = SPMD_DATA.get_unmatched_rank_inits();
        // for(auto call : unmatched_rank) {
        //     llvm::errs() << "=====================================================\n";
        //     auto &D = Context.getDiagnostics();
        //     // TODO warning arrow to rank var instead function call
        //     D.Report(call->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Warning, "Function call with untagged rank variable"));
        // }
        // auto unmatched_size = SPMD_DATA.get_unmatched_size_inits();
        // for(auto call : unmatched_size) {
        //     llvm::errs() << "=====================================================\n";
        //     auto &D = Context.getDiagnostics();
        //     // TODO warning arrow to rank var instead function call
        //     D.Report(call->getBeginLoc(), D.getCustomDiagID(DiagnosticsEngine::Warning, "Function call with untagged size variable"));
        // }
        auto uninitialized_rank = SPMD_DATA.get_uninitialized_rank_vars();
        for(auto var : uninitialized_rank) {
            llvm::errs() << "=====================================================\n";
            auto &D = Context.getDiagnostics();
            D.Report(var->getLocation(), D.getCustomDiagID(DiagnosticsEngine::Warning, "Tagged Rank variable is never initialized with MPI_Comm_rank"));
        }
        auto uninitialized_size = SPMD_DATA.get_uninitialized_size_vars();
        for(auto var : uninitialized_size) {
            llvm::errs() << "=====================================================\n";
            auto &D = Context.getDiagnostics();
            D.Report(var->getLocation(), D.getCustomDiagID(DiagnosticsEngine::Warning, "Tagged Size variable is never initialized with MPI_Comm_size"));
        }
        
        for(auto &call : SPMD_DATA.mpi_calls()) {
            if(call.category == MPIFuncs::Unknown){
                warning_print("Unknown MPIFuncs category for:");
                call.func_decl->dump();
            }
        }

    }
    
    void HandleTranslationUnit(ASTContext &Context) override
    {
        const bool DEBUG = HANDLE_TRANSLATION_UNIT_DEBUG;
        llvm::errs() << "=========================\n";
        llvm::errs() << "= SPMDPlugin is running =\n";
        llvm::errs() << "=========================\n";
        SPMD_DATA.set_context(&Context);

        SPMD_DATA.read_config();

        _match_finder.matchAST(Context);

        if(DEBUG) {
            llvm::errs() << "# of filtered out CFGs: " << FILTERED_CFGS << "\n";
        }

        // match_spmd_vars(Context);

        if(DEBUG) {
            SPMD_DATA.print_cfg(&Context);  
        }

        // TODO WIP PROFILING
        llvm::TimeTraceScope TimeScope("SPMDClang");
        
        // CommunicationGraph cg = CommunicationGraph(&SPMD_DATA);
        auto cg2 = CommunicationGraph(SPMD_DATA);
    }

  private:
    ast_matchers::MatchFinder _match_finder;
    // SPMDAttrCallback _spmd_handler;
    CFGCallback _cfg_handler;
    MPICallback _mpi_handler;
    SPMDVarCallback _spmd_var_handler;
};

/// Plugin entry function
class SPMDAction : public PluginASTAction
{
  public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   llvm::StringRef InFile) override
    {
        return std::make_unique<SPMDConsumer>(&CI.getASTContext());
    }

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) override
    {
        // TODO
        return true;
    }

    ActionType getActionType() override { return ActionType::AddBeforeMainAction; }
};

} // namespace

static FrontendPluginRegistry::Add<SPMDAction> X("spmd-plugin", "Clang SPMD Plugin");
