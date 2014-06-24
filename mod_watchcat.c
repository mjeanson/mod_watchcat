/*
** $Id: mod_watchcat.c,v 1.16 2004/02/16 11:45:36 michel Exp $
** mod_watchcat - Apache module for watchcat service
** See copyright notice in distro's COPYRIGHT file
*/

#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#include <http_request.h>

#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_hash.h>

#include <assert.h>

#include <watchcat.h>

#define DEF_NAME    "default"
#define UNSET       (-1)

struct wcat_type {
    char *name;
    apr_table_t *handlers;
    int timeout;
};

struct wcat_dir_cfg {
    apr_hash_t *types;
    apr_array_header_t *order;
    int log_handler;
};

static void *wcat_create_dir_config(apr_pool_t *p, char *dirspec)
{
    struct wcat_dir_cfg *cfg;
    struct wcat_type *type;

    cfg = apr_palloc(p, sizeof(struct wcat_dir_cfg));
    assert(cfg);
    cfg->types = apr_hash_make(p);
    assert(cfg->types);
    cfg->order = NULL;
    cfg->log_handler = 0;

    type = apr_palloc(p, sizeof(struct wcat_type));
    assert(type);
    type->name = DEF_NAME;
    type->handlers = NULL;
    type->timeout = UNSET;
    apr_hash_set(cfg->types, type->name, APR_HASH_KEY_STRING, type);
    
    return cfg;
}
    
static struct wcat_type *make_type(apr_pool_t *p, const char *name,
                                   struct wcat_type *base)
{
    struct wcat_type *type;
    type = apr_palloc(p, sizeof(struct wcat_type));
    assert(type);
    type->name = apr_pstrdup(p, name);
    assert(type->name);
    
    if (base) {
        if (base->handlers)
            type->handlers = apr_table_copy(p, base->handlers);
        else
            type->handlers = NULL;
        type->timeout = base->timeout;
    }
    else {
        type->handlers = apr_table_make(p, 0);
        assert(type->handlers);
        type->timeout = UNSET;
    }
    return type;
}

static apr_hash_t *copy_types(apr_pool_t *p, apr_hash_t *types)
{
    apr_hash_index_t *hi;
    apr_hash_t *ret;
   
    ret = apr_hash_make(p);
    assert(ret);
    for (hi = apr_hash_first(p, types); hi; hi = apr_hash_next(hi)) {
        struct wcat_type *type;
        struct wcat_type *new;
	    void *val;
        apr_hash_this(hi, NULL, NULL, &val);
        type = (struct wcat_type *)val;
        new = make_type(p, type->name, type); 
        assert(new);
        apr_hash_set(ret, new->name, APR_HASH_KEY_STRING, new);
    }
    return ret;
}

static void overlay_types(apr_pool_t *p, apr_hash_t *overlay, apr_hash_t *base)
{
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(p, base); hi; hi = apr_hash_next(hi)) {
        struct wcat_type *type;
        struct wcat_type *over;
	    void *val;
        apr_hash_this(hi, NULL, NULL, &val);
	    type = (struct wcat_type *)val;
        over = apr_hash_get(overlay, type->name, APR_HASH_KEY_STRING);
        if (over) {
            if (over->handlers) { 
                /* over isn't the default type. */
                assert(type->handlers);
                over->handlers = apr_table_overlay(p, over->handlers,
                                                   type->handlers);
            }
            else
                assert(type->handlers == NULL);
            if (type->timeout != UNSET)
                over->timeout = type->timeout;
        }
        else {
            struct wcat_type *new = make_type(p, type->name, type); 
            assert(new);
            apr_hash_set(overlay, new->name, APR_HASH_KEY_STRING, new);
        }
    }
}

static void *wcat_merge_dir_config(apr_pool_t *p, void *parent_conf,
                                   void *newloc_conf)
{

    struct wcat_dir_cfg *merged = apr_palloc(p, sizeof(struct wcat_dir_cfg));
    struct wcat_dir_cfg *parent = (struct wcat_dir_cfg *) parent_conf;
    struct wcat_dir_cfg *child  = (struct wcat_dir_cfg *) newloc_conf;
    apr_array_header_t *from_order;

    assert(merged);
    merged->log_handler = parent->log_handler || child->log_handler;

    /* Merge merged->types. */
    merged->types = copy_types(p, parent->types);
    overlay_types(p, merged->types, child->types);

    /* Merge merged->order. */
    from_order = child->order ? child->order : parent->order;
    if (from_order) {
        int len = from_order->nelts;
        struct wcat_type **ltypes = (struct wcat_type **)from_order->elts;
        int i;
	    assert(len > 0);
        merged->order = apr_array_make(p, len, sizeof(struct wcat_type *));
        assert(merged->order);
        for (i = 0; i < len; i++) {
            struct wcat_type *type = ltypes[i];
            struct wcat_type **elt = apr_array_push(merged->order);
            assert(elt);
            *elt = apr_hash_get(merged->types, type->name, APR_HASH_KEY_STRING);
            assert(*elt);
        }
    }
    else
        merged->order = NULL;

    return merged;
}

static struct wcat_type *insert_new_type(apr_pool_t *p,
                                         struct wcat_dir_cfg *cfg,
                                         const char *name)
{
    struct wcat_type *type = make_type(p, name, NULL);
    assert(type);
    apr_hash_set(cfg->types, name, APR_HASH_KEY_STRING, type);
    return type;
}

static const char *cmd_types(cmd_parms *cmd, void *mconfig, const char *word1,
                             const char *word2)
{
    struct wcat_dir_cfg *cfg = (struct wcat_dir_cfg *)mconfig;
    struct wcat_type *type;
    assert(cfg);
    type = (struct wcat_type *)apr_hash_get(cfg->types, word1,
                                            APR_HASH_KEY_STRING);
    if (type == NULL)
        type = insert_new_type(cmd->pool, cfg, word1);
    if (type->handlers == NULL) {
        assert(strcmp(type->name, DEF_NAME) == 0);
        return "The special type default doesn't support handlers";
    }
    apr_table_set(type->handlers, word2, "");
    return NULL;
}

static const char *cmd_timeout(cmd_parms *cmd, void *mconfig, const char *word1,
                               const char *word2)
{
    struct wcat_dir_cfg *cfg = (struct wcat_dir_cfg *)mconfig;
    struct wcat_type *type;
    int timeout;

    timeout = atoi(word2);
    if (timeout == LONG_MIN || timeout == LONG_MAX)
        return apr_psprintf(cmd->temp_pool, "Invalid integer (== `%s'): %s",
                            word2, strerror(errno));
    if (timeout < 0)
        return apr_psprintf(cmd->temp_pool, "Invalid timeout (== %i)", timeout);

    assert(cfg);
    type = (struct wcat_type *)apr_hash_get(cfg->types, word1,
                                            APR_HASH_KEY_STRING);
    if (type == NULL)
        type = insert_new_type(cmd->pool, cfg, word1);
    type->timeout = timeout;
    return NULL;
}

static const char *cmd_order(cmd_parms *cmd, void *mconfig, const char *word1)
{
    struct wcat_dir_cfg *cfg = (struct wcat_dir_cfg *)mconfig;
    struct wcat_type *type;
    struct wcat_type **elt;
    assert(cfg);
    type = (struct wcat_type *)apr_hash_get(cfg->types, word1,
                                            APR_HASH_KEY_STRING);
    if (type == NULL)
        type = insert_new_type(cmd->pool, cfg, word1);
    if (cfg->order == NULL)
	    cfg->order = apr_array_make(cmd->pool, 1, sizeof(struct wcat_type *));
    elt = (struct wcat_type **)apr_array_push(cfg->order);
    assert(elt);
    *elt = type;
    return NULL;
}

static const char *cmd_log_handler(cmd_parms *cmd, void *mconfig)
{
    struct wcat_dir_cfg *cfg = (struct wcat_dir_cfg *)mconfig;
    assert(cfg);
    cfg->log_handler = 1;
    return NULL;
}

#define FD_UUID "4a9426a0-5585-11d8-96e2-000347751b8c"

/*
 * Locate our directory configuration record for the current request.
 */

module AP_MODULE_DECLARE_DATA watchcat_module;

static struct wcat_dir_cfg *our_dconfig(const request_rec *r)
{
    return (struct wcat_dir_cfg *)
           ap_get_module_config(r->per_dir_config, &watchcat_module);
}

static request_rec *get_real_req(request_rec *r)
{
    request_rec *prv_r = NULL;
    for (; r; r = r->prev)
        prv_r = r;
    return prv_r;
}

/*
 * This routine is called to perform any module-specific fixing of header
 * fields, et cetera.  It is invoked just before any content-handler.
 *
 * The return value is OK, DECLINED, or HTTP_mumble.  If we return OK, the
 * server will still call any remaining modules with an handler for this
 * phase.
 */
/*
 *  OK ("we did our thing")
 *  DECLINED ("this isn't something with which we want to get involved")
 *  HTTP_mumble ("an error status should be reported")
 */
static int wcat_fixer_upper(request_rec *r)
{
    struct wcat_dir_cfg *cfg;
    int timeout;
    int cat;
    struct wcat_type **ltypes;
    int i, len;
    const char *handler;
    char *info = NULL;
    request_rec *real_r;

    /* Don't handle subrequests. */
    if (r->main)
        return OK;

    cfg = our_dconfig(r);
    assert(cfg);
    
    handler = r->content_type;
    if (handler == NULL)
        handler = r->handler;

    if (cfg->log_handler)
        ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r,
                      "mod_watchcat: handler=`%s' for filename=`%s'",
                      handler, r->filename);

    if (cfg->order == NULL)
        return OK;

    len = cfg->order->nelts;
    assert(len > 0);
    ltypes = (struct wcat_type **)cfg->order->elts;
    timeout = UNSET;
    for (i = 0; i < len; i++) {
        struct wcat_type *type = ltypes[i];
        if (type->handlers == NULL ||
            apr_table_get(type->handlers, handler)) {
            timeout = type->timeout;
            info = type->name;
            if (timeout == UNSET)
                ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r,
                    "mod_watchcat: timeout == UNSET, use CatTimeout directive "
                    "to correct, type=`%s'", type->name);
            break;
        }
    }
    if (timeout == UNSET || timeout <= 0)
        return OK;

    /* Ok, there is work to do! */

    real_r = get_real_req(r);
    assert(real_r);
    
    /* Does the external (real) request already have a cat? */
    if (apr_table_get(real_r->notes, FD_UUID))
        return OK;

    info = apr_psprintf(r->pool, "%s: %s", info, r->filename);
    cat = cat_open1(timeout, SIGKILL, info);
    if (cat >= 0) {
        char *strcat = apr_itoa(r->pool, cat);
        assert(strcat);
        apr_table_set(real_r->notes, FD_UUID, strcat);
        cat_heartbeat(cat);
    }
    else {
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r,
                      "mod_watchcat: cat_open1(%i, SIGKILL, \"%s\") fail",
                      timeout, info);
    }
    return OK;
}

/*
 * This routine is called to perform any module-specific logging activities
 * over and above the normal server things.
 *
 * The return value is OK, DECLINED, or HTTP_mumble.  If we return OK, any
 * remaining modules with an handler for this phase will still be called.
 */
static int wcat_logger(request_rec *r)
{
    const char *strcat;

    if (!ap_is_initial_req(r))
        return OK;
    
    strcat = apr_table_get(r->notes, FD_UUID);
    if (strcat && cat_close(atoi(strcat)) == -1)
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r,
                      "mod_watchcat: cat_close(`%s') fail, strerror(%d)=%s",
                      strcat, errno, strerror(errno));
    return OK;
}

/* 
 * The functions are registered using 
 * ap_hook_foo(name, predecessors, successors, position)
 * where foo is the name of the hook.
 *
 * The args are as follows:
 * name         -> the name of the function to call.
 * predecessors -> a list of modules whose calls to this hook must be
 *                 invoked before this module.
 * successors   -> a list of modules whose calls to this hook must be
 *                 invoked after this module.
 * position     -> The relative position of this module.  One of
 *                 APR_HOOK_FIRST, APR_HOOK_MIDDLE, or APR_HOOK_LAST.
 *                 Most modules will use APR_HOOK_MIDDLE.  If multiple
 *                 modules use the same relative position, Apache will
 *                 determine which to call first.
 *                 If your module relies on another module to run first,
 *                 or another module running after yours, use the 
 *                 predecessors and/or successors.
 */
static void wcat_register_hooks(apr_pool_t *p)
{
    ap_hook_fixups(wcat_fixer_upper, NULL, NULL, APR_HOOK_LAST);
    ap_hook_log_transaction(wcat_logger, NULL, NULL, APR_HOOK_FIRST);
}

static const command_rec wcat_cmds[] =
{
    AP_INIT_ITERATE2("CatType", cmd_types, NULL, OR_OPTIONS,
                     "Insert handlers' name for a type"),
    AP_INIT_TAKE2("CatTimeout", cmd_timeout, NULL, OR_OPTIONS,
                  "Define a timeout value for a type"),
    AP_INIT_ITERATE("CatOrder", cmd_order, NULL, OR_OPTIONS,
                    "Define the match order for the handlers"),
    AP_INIT_NO_ARGS("CatLogHandler", cmd_log_handler, NULL, OR_OPTIONS,
                    "Log the handler of request - no arguments"),
    {NULL}
};

module AP_MODULE_DECLARE_DATA watchcat_module =
{
    STANDARD20_MODULE_STUFF,
    wcat_create_dir_config,     /* Per-directory config creator. */
    wcat_merge_dir_config,      /* Dir config merger. */
    NULL,                       /* Server config creator. */
    NULL,                       /* Server config merger. */
    wcat_cmds,                  /* Command table. */
    wcat_register_hooks,        /* Set up other request processing hooks. */
};
