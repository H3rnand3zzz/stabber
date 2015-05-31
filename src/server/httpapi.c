#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <microhttpd.h>
#include <glib.h>

#include "server/log.h"
#include "server/server.h"

struct MHD_Daemon *httpdaemmon = NULL;

#define POSTBUFFERSIZE 2048

typedef struct conn_info_t {
    GString *body;
} ConnectionInfo;

ConnectionInfo*
create_connection_info(void)
{
    ConnectionInfo *con_info = malloc(sizeof(ConnectionInfo));
    con_info->body = g_string_new("");

    return con_info;
}

void
destroy_connection_info(ConnectionInfo *con_info)
{
    if (con_info->body) {
        g_string_free(con_info->body, TRUE);
        con_info->body = NULL;
    }
    free(con_info);
}

int
send_response(struct MHD_Connection* conn, const char* body, int status_code)
{
    struct MHD_Response* response;
    if (body) {
        response = MHD_create_response_from_data(strlen(body), (void*)body, MHD_NO, MHD_YES);
    } else {
        response = MHD_create_response_from_data(0, NULL, MHD_NO, MHD_YES);
    }

    if (!response)
        return MHD_NO;

    int ret = MHD_queue_response(conn, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

int connection_cb(void* cls, struct MHD_Connection* conn, const char* url, const char* method,
    const char* version, const char* data, size_t* size, void** con_cls)
{
    if (*con_cls == NULL) {
        ConnectionInfo *con_info = create_connection_info();

        if (g_strcmp0(method, "POST") == 0 && g_strcmp0(url, "/send") == 0) {
            if (*size != 0) {
                g_string_append_len(con_info->body, data, *size);
                *size = 0;
            }
        } else {
            return send_response(conn, NULL, MHD_HTTP_BAD_REQUEST);
        }

        *con_cls = (void*) con_info;

        return MHD_YES;
    }

    ConnectionInfo *con_info = (ConnectionInfo*) *con_cls;

    if (*size != 0) {
        g_string_append_len(con_info->body, data, *size);
        *size = 0;

        return MHD_YES;
    } else {
        server_send(con_info->body->str);

        return send_response(conn, NULL, MHD_HTTP_OK);
    }
}

void request_completed(void* cls, struct MHD_Connection* conn,
                       void** con_cls, enum MHD_RequestTerminationCode termcode)
{
    ConnectionInfo *con_info = (ConnectionInfo*) *con_cls;
    if (con_info) {
        destroy_connection_info(con_info);
    }
    con_info = NULL;
    *con_cls = NULL;
}

int
httpapi_start(int port)
{
    httpdaemmon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        port,
        NULL,
        NULL,
        &connection_cb,
        NULL,
        MHD_OPTION_NOTIFY_COMPLETED,
        request_completed,
        NULL, MHD_OPTION_END);

    if (!httpdaemmon) {
        return 0;
    }

    log_println("HTTP daemon started on port: %d", port);

    return 1;
}

void
httpapi_stop(void)
{
    MHD_stop_daemon(httpdaemmon);
    log_println("HTTP daemon stopped.");
}
