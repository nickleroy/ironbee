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
 * @brief IronBee - User Agent Extraction Module
 *
 * This module perform white/black list operations
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

/* Ironbee include files */
#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/debug.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        remote_ip
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();


/**
 * @internal
 * Parse the user agent header, splitting into component fields.
 *
 * Attempt to tokenize the user agent string passed in, storing the
 * result in the DPI associated with the transaction.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modipbl_store_field(ib_engine_t *ib,
                                       ib_mpool_t *mp,
                                       ib_field_t *agent_list,
                                       const char *name,
                                       const char *value)
{
    IB_FTRACE_INIT(modipbl_store_field);
    ib_field_t *tmp_field = NULL;
    ib_status_t rc = IB_OK;

    /* No value?  Do nothing */
    if (value == NULL) {
        ib_log_debug(ib, 9, "No %s field in user agent", name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Create the field */
    rc = ib_field_create(&tmp_field, mp, name, IB_FTYPE_NULSTR, &value);
    if (rc != IB_OK) {
        ib_log_error(ib, 0,
                     "Error creating user agent %s field: %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the field to the list */
    rc = ib_field_list_add(agent_list, tmp_field);
    if (rc != IB_OK) {
        ib_log_error(ib, 0,
                     "Error adding user agent %s field: %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "Stored user agent %s '%s'", name, value);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Parse the user agent header, splitting into component fields.
 *
 * Attempt to tokenize the user agent string passed in, storing the
 * result in the DPI associated with the transaction.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modipbl_agent_fields(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        char *str)
{
    IB_FTRACE_INIT(modipbl_handle_tx);
    ib_field_t *agent_list = NULL;
    char *product = NULL;
    char *platform = NULL;
    char *extra = NULL;
    ib_status_t rc;

    /* Parse the user agent string */
    rc = modipbl_parse_uastring(str, &product, &platform, &extra);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to parse User Agent string '%s'", str);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Build a new list. */
    rc = ib_data_add_list(tx->dpi, "White/Black List", &agent_list);
    if (rc != IB_OK)
    {
        ib_log_error(ib, 0, "Unable to add UserAgent list to DPI.");
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Handle product */
    rc = modipbl_store_field(ib, tx->mp, agent_list, "product", product);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Platform */
    rc = modipbl_store_field(ib, tx->mp, agent_list, "platform", platform);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Extra */
    rc = modipbl_store_field(ib, tx->mp, agent_list, "extra", extra);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle request_header events.
 *
 * Extract the "request_headers" field (a list) from the transactions's
 * data provider instance, then loop through the list, looking for the
 * "User-Agent"  field.  If found, the value is parsed and used to update the
 * connection object fields.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t modipbl_handle_req_headers(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              void *data)
{
    IB_FTRACE_INIT(modipbl_handle_req_headers);
    ib_conn_t *conn = tx->conn;
    ib_field_t *req = NULL;
    ib_status_t rc = IB_OK;
    ib_list_t *lst = NULL;
    ib_list_node_t *node = NULL;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "request_headers", &req);
    if ( (req == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4,
                     "request_headers_event: "
                     "No request headers provided" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* The field value *should* be a list, extract it as such */
    lst = ib_field_value_list(req);
    if (lst == NULL) {
        ib_log_debug(ib, 4,
                     "request_headers_event: "
                     "Field list missing / incorrect type" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Loop through the list; we're looking for User-Agent */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        ib_bytestr_t *bs;
        unsigned len;
        char *buf;

        /* Check the field name
         * Note: field->name is not always a null ('\0') terminated string */
        if (strncmp(field->name, "User-Agent", field->nlen) != 0) {
            continue;
        }

        /* Found it: copy the data into a newly allocated string buffer */
        bs = ib_field_value_bytestr(field);
        len = ib_bytestr_length(bs);

        /* Allocate the memory */
        buf = (char *)ib_mpool_calloc(conn->mp, 1, len+1);
        if (buf == NULL) {
            ib_log_error( ib, 4,
                          "Failed to allocate %d bytes for local address",
                          len+1 );
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy the string out */
        memcpy(buf, ib_bytestr_ptr(bs), len);
        buf[len] = '\0';
        ib_log_debug(ib, 4, "user agent => '%s'", buf);

        /* Finally, split it up & store the components */
        rc = modipbl_agent_fields(ib, tx, buf);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called to initialize the user agent module (when the module is loaded).
 *
 * Registers a handler for the request_headers_event event.
 *
 * @param[in,out] ib IronBee object
 * @param[in] m Module object
 *
 * @returns Status code
 */
static ib_status_t modipbl_init(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modipbl_context_init);
    ib_status_t rc;

    /* Register our callback */
    rc = ib_hook_register(ib, request_headers_event,
                          (ib_void_fn_t)modipbl_handle_req_headers,
                          (void*)request_headers_event);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Hook register returned %d", rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called to close the user agent module (when the module is unloaded).
 *
 * Does nothing
 *
 * @param[in,out] ib IronBee object (unused)
 * @param[in] m Module object (unused)
 *
 * @returns Status code
 */
static ib_status_t modipbl_finish(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modipbl_finish);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called when the module's context is initialized. 
 *
 * Does nothing
 *
 * @param[in,out] ib IronBee object (unused)
 * @param[in] m Module object (unused)
 * @param[in] ctx Configuration context (unused)
 *
 * @returns Status code
 */
static ib_status_t modipbl_context_init(ib_engine_t *ib,
                                        ib_module_t *m,
                                        ib_context_t *ctx)
{
    IB_FTRACE_INIT(modipbl_context_init);

    IB_FTRACE_RET_STATUS(IB_OK);
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    "IP Blacklist",                 /* Module name */
    NULL,                           /* Global config data */
    0,                              /* Global config data length*/
    NULL,                           /* Module config map */
    NULL,                           /* Module directive map */

    modipbl_init,                   /* Initialize function */
    modipbl_finish,                 /* Finish function */
    modipbl_context_init            /* Context init function */
);
