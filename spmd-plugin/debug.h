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
static const bool CONSTRUCT_GRAPH_DEBUG = false;
static const bool COMBINE_PARTIAL_GRAPHS_DEBUG = false;
static const bool COMBINE_PARTIAL_GRAPHS_REC_DEBUG = false;
static const bool INSTANTIATE_FUNC_USES_DEBUG = false;
static const bool VALIDATE_DEBUG = false;
static const bool PRUNE_GRAPH_DEBUG = false;
static const bool DETECT_LOOPS_DEBUG = false;
static const bool DETECT_COMMUNICATION_DEBUG = false;
static const bool ANALYSIS = false;
static const bool CREATE_CG_INSTANCE_DEBUG = false;

//CGNode
static const bool APPEND_ELEMENT_DEBUG = false;

//CGStatementVisitor
static const bool CGSTATEMENT_VISITOR_DEBUG = false;

//CGinstance
static const bool CGINSTANCE_DEBUG = false;

//CGNodeInstance
static const bool CGNODEINSTANCE_APPEND_ELEMENT_DEBUG = false;
static const bool CGNODEINSTANCE_DEBUG = false;

//CGContext
static const bool CGCONTEXT_DEBUG = false;

// CPNBuilder
static const bool CPNBUILDER_GENERAL_DEBUG = false;

// CPNExpression
static const bool CPNEXPRESSION_BUILDING_DEBUG = false;

// CPNStateGraph
static const bool CPNSTATEGRAPH_COMPUTE_DEBUG = false;
static const bool CPNSTATEGRAPH_COMPUTE_MARKING_DEBUG = false;
static const bool CPNSTATEGRAPH_DETECT_ERRORS_DEBUG = false;
static const bool CPNSTATEGRAPH_DISPLAY_STATISTICS_DEBUG = false;

// CPNTransition
static const bool CPNTRANSITION_FIRE_DEBUG = false;
static const bool CPNTRANSITION_VALID_COMBINATIONS_DEBUG = false;


#endif
