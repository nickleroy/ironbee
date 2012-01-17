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

#ifndef _IB_RULE_ENGINE_H_
#define _IB_RULE_ENGINE_H_

/**
 * @file
 * @brief IronBee - Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/types.h>
#include <ironbee/operators.h>
#include <ironbee/list.h>

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
typedef struct ib_action_instance_t ib_action_instance_t;

/**
 * Rule phases.
 */
typedef enum {
    phase_none,
    phase_request_header_available,
    phase_request_body_available,
    phase_response_header_available,
    phase_response_body_available,
    phase_postprocess,
    IB_RULENG_PHASE_COUNT
} ib_ruleng_phase;

/**
 * Rule engine: Rule meta data
 */
typedef struct {
    ib_num_t         number;            /**< Rule number */
    ib_ruleng_phase  phase;             /**< Phase to execute rule */
    void            *data;              /**< Generic data */
} ib_ruleng_rule_meta;

/**
 * Rule engine: condition data
 */
typedef struct {
    ib_operator_instance_t *operator;   /**< Condition operator */
    ib_list_t              *args;       /**< Arguments to the operator */
} ib_ruleng_rule_condition;

/**
 * Rule engine: action */
 */
typedef enum {
    IB_ACTION_RETURN,
} ib_ruleng_rule_action_type;

typedef struct {
    ib_action_instance_t *action;       /**< Action */
    ib_list  _t          *args;         /**< Args to the action function */
} ib_ruleng_rule_action;

/**
 * Rule engine: Rule
 */
typedef struct {
    ib_ruleng_rule_meta        meta;
    ib_ruleng_rule_condition   condition;
    ib_list_t                 *true_actions;
    ib_list_t                 *false_actions; /**< List of action
    ib_ruleng_rulelist_t      *rule_list;     /**< Parent rule list */
} ib_ruleng_rule_t;

/**
 * Rule engine: Rule list
 */
typedef struct {
    ib_list_t                 *rule_list;
} ib_ruleng_rulelist_t;

/**
 * Rule engine: Rule set
 */
typedef struct {
    ib_ruleng_rulelist_t      *rules;
} ib_ruleng_ruleset_t;

/**
 * Register a rule.
 *
 * Register a rule for the rule engine.
 *
 * @param pdst Rule to register
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_register_rule(ib_rule_t *rule);

/**
 * Create a rule.
 *
 * Allocates a rule for the rule engine, initializes it.
 *
 * @param pdst Address which new rule is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_create(ib_rule_t **prule);


#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
