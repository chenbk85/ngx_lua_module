
# Copyright (C) Ngwsx


NGINX_BIN=nginx
NGINX_DIR=../../nginx
ADDON_DIR=$(PWD)

include $(NGINX_DIR)/unix.args

CONF_ARGS= \
	$(CORE_CONF_ARGS) \
	--with-debug \
	$(PCRE_CONF_ARGS) \
	$(HTTP_CONF_ARGS) \
	--without-http-cache \
	--without-http_gzip_module \
	--without-http_auth_basic_module \
	--without-http_proxy_module \
	--without-http_fastcgi_module \
	--without-http_uwsgi_module \
	--without-http_scgi_module \
	--without-http_memcached_module \
	--add-module=$(ADDON_DIR)/../ngx_dbd_module \
	--add-module=$(ADDON_DIR)

include $(NGINX_DIR)/unix.mk
