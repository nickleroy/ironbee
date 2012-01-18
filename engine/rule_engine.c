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

/**
 * @file
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <rule_engine_private.h>
#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>

#include "ironbee_private.h"

/* Callback data */
typedef struct {
    ib_rule_phase_type_t    phase;
    const char             *name;
    ib_state_event_type_t   event;
} rule_cbdata_t;

static rule_cbdata_t rule_cbdata[] = {
    {PHASE_REQUEST_HEADER,  "Request Header",  handle_request_headers_event},
    {PHASE_REQUEST_BODY,    "Request Body",    handle_request_event},
    {PHASE_RESPONSE_HEADER, "Response Header", handle_response_headers_event},
    {PHASE_RESPONSE_BODY,   "Response Body",   handle_response_event},
    {PHASE_POSTPROCESS,     "Post Process",    handle_postprocess_event},
    {PHASE_INVALID,         NULL,              (ib_state_event_type_t) -1}
};

/**
 * @internal
 * Run a set of rules.
 *
 * @param ib Engine
 * @param tx Transaction
 *
 * @returns Status code
 */
static ib_status_t ib_rule_engine_execute(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          void *cbdata)
{
    IB_FTRACE_INIT(ib_rule_engine_execute);
    const rule_cbdata_t *rdata = (const rule_cbdata_t *) cbdata;
    ib_context_t        *ctx = tx->ctx;
    ib_rule_phase_t     *phase = &(ctx->ruleset->phases[rdata->phase]);
    ib_list_t           *rules = &(phase->rules.rule_list);

    /* Sanity check */
    if (phase->phase != rdata->phase) {
        ib_log_error(ib, 4, "Rule engine: Phase %d (%s) is %d",
                     rdata->phase, rdata->name, phase->phase );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_log_debug(ib, 4,
                     "No rules rules for phase %d/%s in context %p",
                     rdata->phase, rdata->name, (void*)ctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_log_debug(ib, 4,
                 "Executing %d rules for phase %d/%s in context %p",
                 IB_LIST_ELEMENTS(rules),
                 rdata->phase, rdata->name, (void*)ctx);
    /* @todo */

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_rule_engine_init);
    rule_cbdata_t *cbdata;
    ib_status_t    rc;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (cbdata = rule_cbdata; cbdata->phase != PHASE_INVALID; ++cbdata) {
        rc = ib_hook_register(ib,
                              cbdata->event,
                              (ib_void_fn_t)ib_rule_engine_execute,
                              (void*)cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for %d/%d/%s returned %d",
                         cbdata->phase, cbdata->event, cbdata->name, rc);
        }
    }

    /* Create the rule list */
    ib->rules = (ib_rulelist_t *)
        ib_mpool_calloc(ib->mp, 1, sizeof(ib_rulelist_t) );
    if (ib->rules == NULL) {
        ib_log_error(ib, 4, "Rule engine failed to allocate rule list");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_rule_engine_ctx_init);
    ib_rule_phase_type_t  phase;       

    /* Create the rule list */
    ctx->ctx_rules = (ib_rulelist_t *)
        ib_mpool_calloc(ctx->mp, 1, sizeof(ib_rulelist_t) );
    if (ib->rules == NULL) {
        ib_log_error(ib, 4, "Rule engine failed to allocate context rule list");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule set */
    ctx->ruleset = (ib_ruleset_t *)
        ib_mpool_calloc(ctx->mp, 1, sizeof(ib_ruleset_t) );
    if (ib->rules == NULL) {
        ib_log_error(ib, 4, "Rule engine failed to allocate context rule list");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    for (phase = PHASE_NONE;  phase < IB_RULE_PHASE_COUNT;  ++phase) {
        ctx->ruleset->phases[phase].phase = phase;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
