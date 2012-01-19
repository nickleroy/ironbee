/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef _IB_RULE_ENGINE_PRIVATE_H_
#define _IB_RULE_ENGINE_PRIVATE_H_

/**
 * @file
 * @brief IronBee - Private rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/operator.h>
#include <ironbee/list.h>
#include <ironbee/rule_engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeRuleEngine Rule Engine
 * @ingroup IronBee
 * @{
 */

/* TODO: in Craig's code */
typedef struct ib_action_t ib_action_t;
typedef struct ib_action_inst_t ib_action_inst_t;

/**
 * Rule engine: Rule meta data
 */
typedef struct {
    const char           *id;           /**< Rule ID */
    ib_rule_phase_type_t  phase;        /**< Phase to execute rule */
    ib_list_t             inputs;       /**< Input field names */
    void                 *data;         /**< Generic data */
} ib_rule_meta_t;

/**
 * Rule engine: condition data
 */
typedef struct {
    ib_operator_instance_t *operator;   /**< Condition operator */
    ib_list_t               args;       /**< Arguments to the operator */
} ib_rule_condition_t;

/**
 * Rule engine: action
 */
typedef struct {
    ib_action_inst_t      *action;        /**< Action */
    ib_list_t              args;          /**< Args to the action function */
} ib_rule_rule_action_t;

/**
 * Rule engine: Rule list
 */
typedef struct {
    ib_list_t              rule_list;     /**< List of rules */
} ib_rulelist_t;

/**
 * Rule engine: Rule
 *
 * The typedef of ib_rule_t is done in ironbee/rule_engine.h
 */
struct ib_rule_t {
    ib_rule_meta_t         meta;          /**< Rule meta data */ 
    ib_rule_condition_t    condition;     /**< Rule condition */
    ib_list_t              true_actions;  /**< Actions if condition True */
    ib_list_t              false_actions; /**< Actions if condition False */
    ib_rulelist_t         *parent_rlist;  /**< Parent rule list */
};

/**
 * Rule engine: List of rules to execute during a phase
 */
typedef struct {
    ib_rule_phase_type_t   phase;        /**< Phase type */
    ib_rulelist_t          rules;        /**< Rules to exececute in a phase */
} ib_rule_phase_t;

/**
 * Rule engine: Set of rules for all phases
 */
typedef struct {
    ib_rule_phase_t        phases[IB_RULE_PHASE_COUNT];
} ib_ruleset_t;


/**
 * @internal
 * Initialize the rule engine.
 *
 * Called when the rule engine is loaded, registers event handlers.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_rule_engine_init(ib_engine_t *ib, ib_module_t *mod);

/**
 * @internal
 * Initialize a context the rule engine.
 *
 * Called when a context is initialized, performs rule engine initializations.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx);


#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
