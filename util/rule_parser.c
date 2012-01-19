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
 * @brief IronBee - Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/types.h>
#include <ironbee/util.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>
#include <ironbee/rule_parser.h>

/* Parse rule operator */
ib_status_t ib_rule_parse_operator(ib_rule_t *rule,
                                   const char *op_str)
                                   
{
    IB_FTRACE_INIT(ib_rule_parse_operator);
    ib_status_t rc = IB_OK;

    /* @todo */
    

    IB_FTRACE_RET_STATUS(rc);
}

/* Parse rule inputs */
ib_status_t ib_rule_parse_inputs(ib_rule_t *rule,
                                 const char *input_str)
                                 
{
    IB_FTRACE_INIT(ib_rule_parse_inputs);
    ib_status_t rc = IB_OK;

    /* @todo */

    IB_FTRACE_RET_STATUS(rc);
}

/* Parse rule modfier */
ib_status_t ib_rule_parse_modifier(ib_rule_t *rule,
                                   const char *modifier_str)
                                   
{
    IB_FTRACE_INIT(ib_rule_parse_modifier);
    ib_status_t rc = IB_OK;

    /* @todo */

    IB_FTRACE_RET_STATUS(rc);
}
