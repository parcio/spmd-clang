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

#ifndef DEBUG_H
#define DEBUG_H

// SPMDPlugin
static const bool HANDLE_TRANSLATION_UNIT_DEBUG = false;

// CommunicationGraph
static const bool CONSTRUCT_GRAPH_DEBUG = true;
static const bool COMBINE_PARTIAL_GRAPHS_DEBUG = true;
static const bool COMBINE_PARTIAL_GRAPHS_REC_DEBUG = true;
static const bool INSTANTIATE_FUNC_USES_DEBUG = true;
static const bool VALIDATE_DEBUG = true;
static const bool PRUNE_GRAPH_DEBUG = true;
static const bool DETECT_LOOPS_DEBUG = true;
static const bool DETECT_COMMUNICATION_DEBUG = true;
static const bool ANALYSIS = true;
static const bool CREATE_CG_INSTANCE_DEBUG = true;

//CGNode
static const bool APPEND_ELEMENT_DEBUG = true;

//CGStatementVisitor
static const bool CGSTATEMENT_VISITOR_DEBUG = true;

//CGinstance
static const bool CGINSTANCE_DEBUG = true;

//CGNodeInstance
static const bool CGNODEINSTANCE_APPEND_ELEMENT_DEBUG = true;
static const bool CGNODEINSTANCE_DEBUG = true;

//CGContext
static const bool CGCONTEXT_DEBUG = true;

// CPNBuilder
static const bool CPNBUILDER_GENERAL_DEBUG = true;

// CPNExpression
static const bool CPNEXPRESSION_BUILDING_DEBUG = true;

// CPNStateGraph
static const bool CPNSTATEGRAPH_COMPUTE_DEBUG = true;
static const bool CPNSTATEGRAPH_COMPUTE_MARKING_DEBUG = true;
static const bool CPNSTATEGRAPH_DETECT_ERRORS_DEBUG = true;
static const bool CPNSTATEGRAPH_DISPLAY_STATISTICS_DEBUG = true;

// CPNTransition
static const bool CPNTRANSITION_FIRE_DEBUG = true;
static const bool CPNTRANSITION_VALID_COMBINATIONS_DEBUG = true;


#endif
