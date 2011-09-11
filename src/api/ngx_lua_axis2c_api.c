
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_lua_module.h>


#if (NGX_WIN32)
#undef S_IWRITE
#endif


#include <axis2_util.h>
#include <axutil_error_default.h>
#include <axiom.h>
#include <axiom_soap.h>


#if !(NGX_WIN32)
#define ngx_stdcall
#endif


typedef struct {
    axutil_allocator_t     allocator;
    ngx_http_request_t    *r;
} ngx_lua_axis2c_allocator_t;


typedef struct {
    axutil_log_t           log;
    ngx_http_request_t    *r;
} ngx_lua_axis2c_log_t;


static int ngx_lua_axis2c_parse(lua_State *l);
static void ngx_lua_axis2c_parse_children(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent, axiom_element_t *parent_elem);

static int ngx_lua_axis2c_serialize(lua_State *l);
static void ngx_lua_axis2c_serialize_tables(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent);
static void ngx_lua_axis2c_serialize_table(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent, char *name, int index);

static axutil_allocator_t *ngx_lua_axis2c_allocator_create(
    ngx_http_request_t *r);
static void *ngx_stdcall ngx_lua_axis2c_allocator_malloc(
    axutil_allocator_t *allocator, size_t size);
static void *ngx_stdcall ngx_lua_axis2c_allocator_realloc(
    axutil_allocator_t *allocator, void *ptr, size_t size);
static void ngx_stdcall ngx_lua_axis2c_allocator_free(
    axutil_allocator_t *allocator, void *ptr);

static axutil_log_t *ngx_lua_axis2c_log_create(ngx_http_request_t *r);
static void ngx_stdcall ngx_lua_axis2c_log_free(axutil_allocator_t *allocator,
    axutil_log_t *log);
static void ngx_stdcall ngx_lua_axis2c_log_write(axutil_log_t *log,
    const axis2_char_t *buffer, axutil_log_levels_t level,
    const axis2_char_t *file, const int line);


static axutil_log_ops_t  ngx_lua_axis2c_log_ops = {
    ngx_lua_axis2c_log_free,
    ngx_lua_axis2c_log_write
};


static ngx_lua_const_t  ngx_lua_axis2c_consts[] = {
    { NULL, 0 }
};


static luaL_Reg  ngx_lua_axis2c_methods[] = {
    { "parse", ngx_lua_axis2c_parse },
    { "serialize", ngx_lua_axis2c_serialize },
    { NULL, NULL }
};


void
ngx_lua_axis2c_api_init(lua_State *l)
{
    int  n;

    n = sizeof(ngx_lua_axis2c_consts) / sizeof(ngx_lua_const_t) - 1;
    n += sizeof(ngx_lua_axis2c_methods) / sizeof(luaL_Reg) - 1;

    lua_createtable(l, 0, n);

    for (n = 0; ngx_lua_axis2c_consts[n].name != NULL; n++) {
        lua_pushinteger(l, ngx_lua_axis2c_consts[n].value);
        lua_setfield(l, -2, ngx_lua_axis2c_consts[n].name);
    }

    for (n = 0; ngx_lua_axis2c_methods[n].name != NULL; n++) {
        lua_pushcfunction(l, ngx_lua_axis2c_methods[n].func);
        lua_setfield(l, -2, ngx_lua_axis2c_methods[n].name);
    }

    lua_setfield(l, -2, "axis2c");

    axutil_error_init();
}


static int
ngx_lua_axis2c_parse(lua_State *l)
{
    char                   *uri, *prefix, *name;
    ngx_str_t               soap;
    axiom_node_t           *node;
    axutil_env_t           *env;
    axutil_log_t           *log;
    axutil_error_t         *error;
    axiom_element_t        *elem;
    axiom_namespace_t      *ns;
    axiom_soap_body_t      *body;
    axutil_allocator_t     *a;
    ngx_http_request_t     *r;
    axiom_xml_reader_t     *reader;
    axiom_soap_header_t    *header;
    axiom_stax_builder_t   *builder;
    axiom_soap_builder_t   *soap_builder;
    axiom_soap_envelope_t  *envelope;

    r = ngx_lua_request(l);

    soap.data = (u_char *) luaL_checklstring(l, -1, &soap.len);

    lua_createtable(l, 2, 2);

    a = ngx_lua_axis2c_allocator_create(r);
    log = ngx_lua_axis2c_log_create(r);
    error = axutil_error_create(a);
    env = axutil_env_create_with_error_log(a, error, log);

    reader = axiom_xml_reader_create_for_memory(env, soap.data, soap.len, NULL,
                                                AXIS2_XML_PARSER_TYPE_BUFFER);
    builder = axiom_stax_builder_create(env, reader);
    soap_builder = axiom_soap_builder_create(env, builder,
                                      AXIOM_SOAP12_SOAP_ENVELOPE_NAMESPACE_URI);
    envelope = axiom_soap_builder_get_soap_envelope(soap_builder, env);

    ns = axiom_soap_envelope_get_namespace(envelope, env);
    if (ns != NULL) {
        uri = axiom_namespace_get_uri(ns, env);
        if (uri != NULL) {
            lua_pushstring(l, uri);
            lua_setfield(l, -2, "uri");
        }

        prefix = axiom_namespace_get_prefix(ns, env);
        if (prefix != NULL) {
            lua_pushstring(l, prefix);
            lua_setfield(l, -2, "prefix");
        }
    }

    header = axiom_soap_envelope_get_header(envelope, env);
    if (header != NULL) {
        node = axiom_soap_header_get_base_node(header, env);
        elem = axiom_node_get_data_element(node, env);

        /* TODO */

        lua_newtable(l);

        ngx_lua_axis2c_parse_children(l, env, node, elem);

        lua_setfield(l, -2, "header");
    }

    body = axiom_soap_envelope_get_body(envelope, env);
    if (body != NULL) {

        /* TODO: axiom_soap_body_has_fault */
        /* TODO: axiom_soap_body_get_fault */

        node = axiom_soap_body_get_base_node(body, env);
        if (node == NULL) {
            return 1;
        }

        if (axiom_node_get_node_type(node, env) != AXIOM_ELEMENT) {
            return 1;
        }

        elem = axiom_node_get_data_element(node, env);
        name = axiom_element_get_localname(elem, env);

        if (ngx_strcmp(name, "Body") != 0) {
            return 1;
        }

        lua_newtable(l);

        ngx_lua_axis2c_parse_children(l, env, node, elem);

        lua_setfield(l, -2, "body");
    }

    return 1;
}


static void
ngx_lua_axis2c_parse_children(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent, axiom_element_t *parent_elem)
{
    int                              n;
    char                            *uri, *prefix, *name, *text, *value;
    axiom_node_t                    *node;
    axutil_hash_t                   *attrs;
    axiom_element_t                 *elem;
    axiom_attribute_t               *attr;
    axiom_namespace_t               *ns;
    axutil_hash_index_t             *hi;
    axiom_child_element_iterator_t  *it;

    it = axiom_element_get_child_elements(parent_elem, env, parent);
    if (it == NULL) {
        return;
    }

    n = 1;

    do {

        node = axiom_child_element_iterator_next(it, env);
        elem = axiom_node_get_data_element(node, env);
        name = axiom_element_get_localname(elem, env);

        lua_createtable(l, 2, 4);

        lua_pushstring(l, name);
        lua_setfield(l, -2, "name");

        ns = axiom_element_get_namespace(elem, env, node);
        if (ns != NULL) {
            uri = axiom_namespace_get_uri(ns, env);
            if (uri != NULL) {
                lua_pushstring(l, uri);
                lua_setfield(l, -2, "uri");
            }

            prefix = axiom_namespace_get_prefix(ns, env);
            if (prefix != NULL) {
                lua_pushstring(l, prefix);
                lua_setfield(l, -2, "prefix");
            }
        }

        attrs = axiom_element_get_all_attributes(elem, env);
        if (attrs != NULL) {
            lua_newtable(l);

            hi = axutil_hash_first(attrs, env);

            do {
                if (hi == NULL) {
                    break;
                }

                axutil_hash_this(hi, NULL, NULL, &attr);

                name = axiom_attribute_get_localname(attr, env);
                value = axiom_attribute_get_value(attr, env);

                lua_pushstring(l, value);
                lua_setfield(l, -2, name);

                hi = axutil_hash_next(env, hi);
            } while (1);

            lua_setfield(l, -2, "attributes");
        }

        text = axiom_element_get_text(elem, env, node);
        if (text != NULL) {
            lua_pushstring(l, text);
            lua_setfield(l, -2, "text");

        } else {
            lua_newtable(l);

            ngx_lua_axis2c_parse_children(l, env, node, elem);

            lua_setfield(l, -2, "children");
        }

        lua_setfield(l, -2, name);

        lua_getfield(l, -1, name);
        lua_rawseti(l, -2, n++);

    } while (axiom_child_element_iterator_has_next(it, env) == AXIS2_TRUE);
}


static int
ngx_lua_axis2c_serialize(lua_State *l)
{
    int                     top;
    char                   *uri, *prefix;
    axiom_node_t           *node;
    axutil_env_t           *env;
    axutil_log_t           *log;
    axutil_error_t         *error;
    axiom_output_t         *output;
    axiom_namespace_t      *ns;
    axiom_soap_body_t      *body;
    axutil_allocator_t     *a;
    ngx_http_request_t     *r;
    axiom_xml_writer_t     *writer;
    axiom_soap_header_t    *header;
    axiom_soap_envelope_t  *envelope;

    r = ngx_lua_request(l);

    top = lua_gettop(l);

    if (!lua_istable(l, top)) {
        return luaL_error(l, "invalid argument, must be a table");
    }

    a = ngx_lua_axis2c_allocator_create(r);
    log = ngx_lua_axis2c_log_create(r);
    error = axutil_error_create(a);
    env = axutil_env_create_with_error_log(a, error, log);

    lua_getfield(l, top, "uri");
    uri = (char *) luaL_optstring(l, -1,
                                  "http://www.w3.org/2003/05/soap-envelope");

    lua_getfield(l, top, "prefix");
    prefix = (char *) luaL_optstring(l, -1, "soap");

    ns = axiom_namespace_create(env, uri, prefix);
    envelope = axiom_soap_envelope_create(env, ns);

    lua_getfield(l, top, "header");
    if (!lua_isnil(l, -1)) {
        if (!lua_istable(l, -1)) {
            return luaL_error(l, "the value of \"header\" must be a table");
        }

        header = axiom_soap_header_create_with_parent(env, envelope);
        node = axiom_soap_header_get_base_node(header, env);

        ngx_lua_axis2c_serialize_tables(l, env, node);
    }

    lua_getfield(l, top, "body");
    if (!lua_isnil(l, -1)) {
        if (!lua_istable(l, -1)) {
            return luaL_error(l, "the value of \"body\" must be a table");
        }

        body = axiom_soap_body_create_with_parent(env, envelope);
        node = axiom_soap_body_get_base_node(body, env);

        ngx_lua_axis2c_serialize_tables(l, env, node);
    }

    lua_settop(l, top);

    writer = axiom_xml_writer_create_for_memory(env, NULL, AXIS2_FALSE,
                                                AXIS2_FALSE,
                                                AXIS2_XML_PARSER_TYPE_BUFFER);
    output = axiom_output_create(env, writer);
    axiom_soap_envelope_serialize(envelope, env, output, AXIS2_FALSE);

    lua_pushstring(l, axiom_xml_writer_get_xml(writer, env));

    return 1;
}


static void
ngx_lua_axis2c_serialize_tables(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent)
{
    int    type, n, i, index;
    char  *name;

    lua_pushnil(l);

    while (lua_next(l, -2)) {
        type = lua_type(l, -2);
        if (type == LUA_TNUMBER) {
            lua_pop(l, 1);
            continue;
        }

        name = (char *) luaL_checkstring(l, -2);

        if (!lua_istable(l, -1)) {
            luaL_error(l, "the value of \"%s\" must be a table", name);
        }

        index = lua_gettop(l);

        ngx_lua_axis2c_serialize_table(l, env, parent, name, index);

        lua_pop(l, 1);
    }

    n = lua_objlen(l, -1);

    for (i = 1; i <= n; i++) {
        lua_rawgeti(l, -1, i);

        /* TODO */

        if (!lua_istable(l, -1)) {
            luaL_error(l, "must be a table");
        }

        lua_getfield(l, -1, "name");
        name = (char *) luaL_checkstring(l, -1);

        index = lua_gettop(l) - 1;

        ngx_lua_axis2c_serialize_table(l, env, parent, name, index);

        lua_pop(l, 2);
    }
}


static void
ngx_lua_axis2c_serialize_table(lua_State *l, axutil_env_t *env,
    axiom_node_t *parent, char *name, int index)
{
    int                 top;
    char               *uri, *prefix, *text, *value;
    axiom_node_t       *node;
    axiom_element_t    *elem;
    axiom_namespace_t  *ns;
    axiom_attribute_t  *attr;

    top = lua_gettop(l);

    lua_getfield(l, index, "uri");
    uri = (char *) luaL_optstring(l, -1, NULL);

    lua_getfield(l, index, "prefix");
    prefix = (char *) luaL_optstring(l, -1, NULL);

    if (uri != NULL || prefix != NULL) {
        ns = axiom_namespace_create(env, uri, prefix);

    } else {
        ns = NULL;
    }

    elem = axiom_element_create(env, parent, name, ns, &node);

    lua_getfield(l, index, "attributes");
    if (!lua_isnil(l, -1)) {
        if (!lua_istable(l, -1)) {
            luaL_error(l, "the value of \"attributes\" must be a table");
        }

        lua_pushnil(l);

        while (lua_next(l, -2)) {
            name = (char *) luaL_checkstring(l, -2);
            value = (char *) luaL_checkstring(l, -1);

            attr = axiom_attribute_create(env, name, value, NULL);
            axiom_element_add_attribute(elem, env, attr, node);

            lua_pop(l, 1);
        }
    }

    lua_getfield(l, index, "text");
    text = (char *) luaL_optstring(l, -1, NULL);

    if (text != NULL) {
        axiom_element_set_text(elem, env, text, node);
        lua_settop(l, top);
        return;
    }

    lua_getfield(l, index, "children");
    if (!lua_isnil(l, -1)) {
        if (!lua_istable(l, -1)) {
            luaL_error(l, "the value of \"children\" must be a table");
        }

        ngx_lua_axis2c_serialize_tables(l, env, node);
    }

    lua_settop(l, top);
}


static axutil_allocator_t *
ngx_lua_axis2c_allocator_create(ngx_http_request_t *r)
{
    ngx_lua_axis2c_allocator_t  *a;

    a = ngx_pcalloc(r->pool, sizeof(ngx_lua_axis2c_allocator_t));
    if (a == NULL) {
        return NULL;
    }

    a->allocator.malloc_fn = ngx_lua_axis2c_allocator_malloc;
    a->allocator.realloc = ngx_lua_axis2c_allocator_realloc;
    a->allocator.free_fn = ngx_lua_axis2c_allocator_free;
    a->r = r;

    return &a->allocator;
}


static void *ngx_stdcall
ngx_lua_axis2c_allocator_malloc(axutil_allocator_t *allocator, size_t size)
{
    ngx_lua_axis2c_allocator_t *a = (ngx_lua_axis2c_allocator_t *) allocator;

    u_char  *p;

    p = ngx_palloc(a->r->pool, size + sizeof(size_t));
    if (p == NULL) {
        return NULL;
    }

    *((size_t *) p) = size;
    p += sizeof(size_t);

    return p;
}


static void *ngx_stdcall
ngx_lua_axis2c_allocator_realloc(axutil_allocator_t *allocator, void *ptr,
    size_t size)
{
    size_t   osize;
    u_char  *p;

    p = (u_char *) ptr - sizeof(size_t);
    osize = *((size_t *) p);

    if (osize >= size) {
        return ptr;
    }

    p = ngx_lua_axis2c_allocator_malloc(allocator, size);
    ngx_memcpy(p, ptr, osize);
    ngx_lua_axis2c_allocator_free(allocator, ptr);

    return p;
}


static void ngx_stdcall
ngx_lua_axis2c_allocator_free(axutil_allocator_t *allocator, void *ptr)
{
    ngx_lua_axis2c_allocator_t *a = (ngx_lua_axis2c_allocator_t *) allocator;

    size_t   size;
    u_char  *p;

    p = (u_char *) ptr - sizeof(size_t);
    size = *((size_t *) p) + sizeof(size_t);

    if (size > a->r->pool->max) {
        ngx_pfree(a->r->pool, p);
    }
}


static axutil_log_t *
ngx_lua_axis2c_log_create(ngx_http_request_t *r)
{
    ngx_lua_axis2c_log_t  *log;

    log = ngx_pcalloc(r->pool, sizeof(ngx_lua_axis2c_log_t));
    if (log == NULL) {
        return NULL;
    }

    log->log.ops = &ngx_lua_axis2c_log_ops;
    log->log.level = AXIS2_LOG_LEVEL_TRACE;
    log->log.enabled = 1;
    log->r = r;

    return &log->log;
}


static void ngx_stdcall
ngx_lua_axis2c_log_free(axutil_allocator_t *allocator, axutil_log_t *log)
{
}


static void ngx_stdcall
ngx_lua_axis2c_log_write(axutil_log_t *log, const axis2_char_t *buffer,
    axutil_log_levels_t level, const axis2_char_t *file, const int line)
{
    ngx_lua_axis2c_log_t *l = (ngx_lua_axis2c_log_t *) log;

    /* TODO */

    ngx_log_error(NGX_LOG_ALERT, l->r->connection->log, 0, buffer);
}
