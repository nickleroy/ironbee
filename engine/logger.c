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
 * @brief IronBee &mdash; Logger
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>

#include <ironbee/engine.h>
#include <ironbee/core.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>
#include <ironbee/clock.h>

#include "ironbee_private.h"


/* -- Internal Routines -- */

/**
 * Engine default logger.
 *
 * This is the default logger that executes when no other logger has
 * been configured.
 *
 * @param fp File pointer
 * @param level Log level
 * @param tx Transaction information (or NULL)
 * @param prefix Optional prefix to the log
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 * @param ap Variable argument list
 */
static void default_logger(FILE *fp, int level,
                           const ib_tx_t *tx,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    IB_FTRACE_INIT();
    char fmt2[1024 + 1];
    char tx_info[1024 + 1];
    char time_info[32 + 1];
    struct tm *tminfo;
    time_t timet;
    int ec = 0;

    if (level > 4) {
        IB_FTRACE_RET_VOID();
    }

    if (tx != NULL) {
        ec = snprintf(tx_info, 1024,
                      "[tx:%s] ",
                      tx->id+31);
    }
    else {
        tx_info[0] = '\0';
    }
    if (ec > 1024) {
        abort();
    }

    timet = time(NULL);
    tminfo = localtime(&timet);
    strftime(time_info, sizeof(time_info)-1, "%d%m%Y.%Hh%Mm%Ss", tminfo);

    if ((file != NULL) && (line > 0)) {
        ec = snprintf(fmt2, 1024,
                      "%s %s[%d] (%s:%d) %s%s\n",
                      time_info,
                      (prefix?prefix:""), level, file, line, tx_info, fmt);
    }
    else {
        ec = snprintf(fmt2, 1024,
                      "%s %s[%d] %s%s\n",
                      time_info,
                      (prefix?prefix:""), level, tx_info, fmt);
    }
    if (ec > 1024) {
        /// @todo Do something better
        abort();
    }


    vfprintf(fp, fmt2, ap);
    fflush(fp);
    IB_FTRACE_RET_VOID();
}


/* -- Log Event Routines -- */

/** Log Event Types */
const char *ib_logevent_type_str[] = {
    "Unknown",
    "Observation",
    NULL
};


/** Log Event Action Names */
static const char *ib_logevent_action_str[] = {
    "Unknown",
    "Log",
    "Block",
    "Ignore",
    "Allow",
    NULL
};

const char *ib_logevent_type_name(ib_logevent_type_t num)
{
    if (num >= (sizeof(ib_logevent_type_str) / sizeof(const char *))) {
        return ib_logevent_type_str[0];
    }
    return ib_logevent_type_str[num];
}

const char *ib_logevent_action_name(ib_logevent_action_t num)
{
    if (num >= (sizeof(ib_logevent_action_str) / sizeof(const char *))) {
        return ib_logevent_action_str[0];
    }
    return ib_logevent_action_str[num];
}

ib_status_t DLL_PUBLIC ib_logevent_create(ib_logevent_t **ple,
                                          ib_mpool_t *pool,
                                          const char *rule_id,
                                          ib_logevent_type_t type,
                                          ib_logevent_action_t rec_action,
                                          ib_logevent_action_t action,
                                          uint8_t confidence,
                                          uint8_t severity,
                                          const char *fmt,
                                          ...)
{
    IB_FTRACE_INIT();

    /*
     * Defined so that size_t to int cast is avoided
     * checking the result of vsnprintf below.
     * NOTE: This is assumed >3 bytes and should not
     *       be overly large as it is used as the size
     *       of a stack buffer.
     */
#define IB_LEVENT_MSG_BUF_SIZE 1024

    char buf[IB_LEVENT_MSG_BUF_SIZE];
    va_list ap;

    *ple = (ib_logevent_t *)ib_mpool_calloc(pool, 1, sizeof(**ple));
    if (*ple == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*ple)->event_id = (uint32_t)ib_clock_get_time(); /* truncated */
    (*ple)->mp = pool;
    (*ple)->rule_id = ib_mpool_strdup(pool, rule_id);
    (*ple)->type = type;
    (*ple)->rec_action = rec_action;
    (*ple)->action = action;
    (*ple)->confidence = confidence;
    (*ple)->severity = severity;

    /*
     * Generate the message, replacing the last three characters
     * with "..." if truncation is required.
     */
    va_start(ap, fmt);
    if (vsnprintf(buf, IB_LEVENT_MSG_BUF_SIZE, fmt, ap) >= IB_LEVENT_MSG_BUF_SIZE) {
        memcpy(buf + (IB_LEVENT_MSG_BUF_SIZE - 3), "...", 3);
    }
    va_end(ap);

    /* Copy the formatted message. */
    (*ple)->msg = ib_mpool_strdup(pool, buf);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_logevent_tag_add(ib_logevent_t *le,
                                           const char *tag)
{
    IB_FTRACE_INIT();
    char *tag_copy;
    ib_status_t rc;

    assert(le != NULL);

    if (le->tags == NULL) {
        rc = ib_list_create(&le->tags, le->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    tag_copy = ib_mpool_memdup(le->mp, tag, strlen(tag) + 1);
    rc = ib_list_push(le->tags, tag_copy);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_logevent_field_add(ib_logevent_t *le,
                                             const char *name)
{
    IB_FTRACE_INIT();
    char *name_copy;
    ib_status_t rc;

    assert(le != NULL);

    if (le->fields == NULL) {
        rc = ib_list_create(&le->fields, le->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    name_copy = ib_mpool_memdup(le->mp, name, strlen(name) + 1);
    rc = ib_list_push(le->fields, name_copy);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_logevent_data_set(ib_logevent_t *le,
                                            const void *data,
                                            size_t dlen)
{
    IB_FTRACE_INIT();

    assert(le != NULL);

    // TODO Copy the data???
    le->data = data;
    le->data_len = dlen;

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Exported Logging Routines -- */

static const char* c_log_levels[] = {
    "EMERGENCY",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "DEBUG2",
    "DEBUG3",
    "TRACE"
};
static size_t c_num_levels = sizeof(c_log_levels)/sizeof(*c_log_levels);

ib_log_level_t ib_log_string_to_level(const char* s)
{
    unsigned int i;

    for (i = 0; i < c_num_levels; ++i) {
        if (
            strncasecmp(s, c_log_levels[i], strlen(c_log_levels[i])) == 0 &&
            strlen(s) == strlen(c_log_levels[i])
        ) {
            break;
        }
    }
    return i;
}

const char *ib_log_level_to_string(ib_log_level_t level)
{
    if (level < c_num_levels) {
        return c_log_levels[level];
    }
    else {
        return "UNKNOWN";
    }
}

ib_provider_inst_t *ib_log_provider_get_instance(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        IB_FTRACE_RET_PTR(ib_provider_inst_t, NULL);
    }

    IB_FTRACE_RET_PTR(ib_provider_inst_t, corecfg->pi.logger);
}

void ib_log_provider_set_instance(ib_context_t *ctx, ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        /// @todo This func should return ib_status_t now
        IB_FTRACE_RET_VOID();
    }

    corecfg->pi.logger = pi;

    IB_FTRACE_RET_VOID();
}

void DLL_PUBLIC ib_log_ex(ib_engine_t *ib, int level,
                           const ib_tx_t *tx,
                           const char *prefix, const char *file, int line,
                           const char *fmt, ...)
{
    IB_FTRACE_INIT();

    va_list ap;
    va_start(ap, fmt);

    ib_vlog_ex(ib, level, tx, prefix, file, line, fmt, ap);

    va_end(ap);

    IB_FTRACE_RET_VOID();
}

void DLL_PUBLIC ib_vlog_ex(ib_engine_t *ib, int level,
                           const ib_tx_t *tx,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    IB_FTRACE_INIT();

    IB_PROVIDER_API_TYPE(logger) *api;
    ib_core_cfg_t *corecfg;
    ib_provider_inst_t *pi = NULL;
    ib_status_t rc;
    ib_context_t *ctx;

    ctx = ib_context_main(ib);

    if (ctx != NULL) {
        rc = ib_context_module_config(ctx, ib_core_module(),
                                      (void *)&corecfg);
        if (rc == IB_OK) {
            pi = corecfg->pi.logger;
        }

        if (pi != NULL) {
            api = (IB_PROVIDER_API_TYPE(logger) *)pi->pr->api;

            api->vlogmsg(pi, ctx, level, tx, prefix, file, line, fmt, ap);

            IB_FTRACE_RET_VOID();
        }
    }

    default_logger(stderr, level, tx, prefix, file, line, fmt, ap);

    IB_FTRACE_RET_VOID();
}

int DLL_PUBLIC ib_log_get_level(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->log_level);
}

void DLL_PUBLIC ib_log_set_level(ib_engine_t *ib, int level)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    corecfg->log_level = level;
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_event_add(ib_provider_inst_t *pi,
                         ib_logevent_t *e)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->add_event(pi, e);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_remove(ib_provider_inst_t *pi,
                            uint32_t id)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->remove_event(pi, id);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_get_all(ib_provider_inst_t *pi,
                             ib_list_t **pevents)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->fetch_events(pi, pevents);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_write_all(ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->write_events(pi);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_auditlog_write(ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(audit) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(audit) *)pi->pr->api;

    rc = api->write_log(pi);
    IB_FTRACE_RET_STATUS(rc);
}
