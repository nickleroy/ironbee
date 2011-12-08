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

#include <math.h>
#include <strings.h>

#include <GeoIPCity.h>

#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/config.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        geoip
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)


/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * The GeoIP database.
 * @endinternal
 */
static GeoIP *geoip_db = NULL;

static void geoip_lookup(ib_engine_t *ib, ib_tx_t *tx, void *data )
{
    IB_FTRACE_INIT(geoip_lookup);
    
    const char *ip = tx->conn->remote_ipstr;
    
    /**
     * Some configurations exist as single characters and must be converted to
     * a string. This is simply a place to assemble that string before
     * it is passed into ip_data_add_nulstr.
     */
    char one_char_str[2] = { '\0', '\0' };

    ib_status_t rc;
    
    /* Declare and initialize the GeoIP property list.
     * Regardless of if we find a record or not, we want to create the list
     * artifact so that later modules know we ran and did [not] find a 
     * record. */
    ib_field_t *geoip_lst = NULL;
    
    ib_field_t *tmp_field = NULL;
    
    ib_num_t longitude;
    ib_num_t latitude;

    GeoIPRecord *geoip_rec;

    ib_log_debug(ib, 4, "GeoIP Lookup");
    
    /* Build a new list. */
    rc = ib_data_add_list(tx->dpi, "GEOIP", &geoip_lst);

    /* NOTICE: Called beofore GeoIP_record_by_addr allocates a 
     * GeoIPRecord. */
    if (rc != IB_OK)
    {
        ib_log_error(ib, 0, "Unable to add GEOIP list to DPI.");
        IB_FTRACE_RET_VOID();
    }

    geoip_rec = GeoIP_record_by_addr(geoip_db, ip);
    
    if (geoip_rec != NULL)
    {
        ib_log_debug(ib, 4, "GeoIP record found.");

        /* Append the floats latitude and longitude.
         * NOTE: Future work may add a float type to the Ironbee DPI. */

        latitude = lround(geoip_rec->latitude);
        ib_field_create(&tmp_field,
                        tx->mp,
                        "latitude",
                        IB_FTYPE_NUM,
                        &latitude);
        ib_field_list_add(geoip_lst, tmp_field);

        longitude = lround(geoip_rec->longitude); 
        ib_field_create(&tmp_field,
                        tx->mp,
                        "longitude",
                        IB_FTYPE_NUM,
                        &longitude);
        ib_field_list_add(geoip_lst, tmp_field);
                        
        /* Add integers. */
        tmp_field = NULL;
        ib_field_create(&tmp_field,
                        tx->mp,
                        "accuracy_radius",
                        IB_FTYPE_NUM,
                        &geoip_rec->accuracy_radius);
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        "area_code",
                        IB_FTYPE_NUM,
                        &geoip_rec->area_code);
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        "charset",
                        IB_FTYPE_NUM,
                        &geoip_rec->charset);
        ib_field_list_add(geoip_lst, tmp_field);
        ib_field_create(&tmp_field,
                        tx->mp,
                        "metro_code",
                        IB_FTYPE_NUM,
                        &geoip_rec->metro_code);
        ib_field_list_add(geoip_lst, tmp_field);

        /* Add strings. */
        if (geoip_rec->country_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "country_code",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->country_code);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->country_code3!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "country_code3",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->country_code3);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->country_name!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "country_name",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->country_name);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->region!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "region",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->region);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->city!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "city",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->city);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->postal_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "postal_code",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->postal_code);
            ib_field_list_add(geoip_lst, tmp_field);
        }

        if (geoip_rec->continent_code!=NULL)
        {
            ib_field_create(&tmp_field,
                            tx->mp,
                            "continent_code",
                            IB_FTYPE_NULSTR,
                            &geoip_rec->continent_code);
            ib_field_list_add(geoip_lst, tmp_field);
        }
                           
        /* Wrap single character arguments into a 2-character string and add. */
        one_char_str[0] = geoip_rec->country_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        "country_conf",
                        IB_FTYPE_NULSTR,
                        &one_char_str);
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->region_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        "region_conf",
                        IB_FTYPE_NULSTR,
                        &one_char_str);
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->city_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        "city_conf",
                        IB_FTYPE_NULSTR,
                        &one_char_str);
        ib_field_list_add(geoip_lst, tmp_field);

        one_char_str[0] = geoip_rec->postal_conf;
        ib_field_create(&tmp_field,
                        tx->mp,
                        "postal_conf",
                        IB_FTYPE_NULSTR,
                        &one_char_str);
        ib_field_list_add(geoip_lst, tmp_field);
        
        GeoIPRecord_delete(geoip_rec);
    }
    else
    {
        ib_log_debug(ib, 4, "No GeoIP record found.");
    }
    
    IB_FTRACE_RET_VOID();
}

static ib_status_t geoip_database_file_dir_param1(ib_cfgparser_t *cp,
                                                  const char *name,
                                                  const char *p1,
                                                  void *cbdata)
{
    IB_FTRACE_INIT(geoip_database_file_dir_param1);

    if (geoip_db != NULL)
    {
        GeoIP_delete(geoip_db);
        geoip_db = NULL;
    }

    IB_FTRACE_MSG("Initializing custom GeoIP database...");
    IB_FTRACE_MSG(p1);

    geoip_db = GeoIP_open(p1, GEOIP_MMAP_CACHE);

    if (geoip_db == NULL)
    {
        IB_FTRACE_MSG("Failed to initialize GeoIP database.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_DIRMAP_INIT_STRUCTURE(geoip_directive_map) = {

    /* Give the config parser a callback for the directive GeoIPDatabaseFile */
    IB_DIRMAP_INIT_PARAM1(
        "GeoIPDatabaseFile",
        geoip_database_file_dir_param1,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

/* Called when module is loaded. */
static ib_status_t geoip_init(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(geoip_init);
    
    ib_status_t rc;
    
    if (geoip_db == NULL)
    {
        ib_log_debug(ib, 4, "Initializing default GeoIP database...");
        geoip_db = GeoIP_new(GEOIP_MMAP_CACHE);
    }

    if (geoip_db == NULL)
    {
        ib_log_debug(ib, 4, "Failed to initialize GeoIP database.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    ib_log_debug(ib, 4, "Initializing GeoIP database complete.");

    ib_log_debug(ib, 4, "Registering handler...");

    rc = ib_hook_register(ib,
                          handle_context_tx_event,
                          (ib_void_fn_t)geoip_lookup,
                          NULL);
                          
    ib_log_debug(ib, 4, "Done registering handler.");
    
    if (rc != IB_OK)
    {
        ib_log_debug(ib, 4, "Failed to load GeoIP module.");
        IB_FTRACE_RET_STATUS(rc);
    }
    
    ib_log_debug(ib, 4, "GeoIP module loaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Called when module is unloaded. */
static ib_status_t geoip_fini(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(geoip_fini);
    if (geoip_db!=NULL)
    {
        GeoIP_delete(geoip_db);
    }
    ib_log_debug(ib, 4, "GeoIP module unloaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    geoip_directive_map,                 /* Config directive map */
    geoip_init,                          /* Initialize function */
    geoip_fini,                          /* Finish function */
    NULL,                                /* Context init function */
    NULL                                 /* Context fini function */
);
