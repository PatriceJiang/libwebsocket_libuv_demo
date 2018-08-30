#include <stdio.h>
#include <iostream>
#include <vector>
#include <libwebsockets.h>
#include <uv.h>

#include "url.h"


#ifndef USE_LIBUV
#define USE_LIBUV 1
#endif // !USE_LIBUV


#ifdef WIN32
#pragma comment( lib, "psapi.lib" )
#pragma comment( lib, "ws2_32.lib")
#pragma comment( lib, "iphlpapi.lib")
#pragma comment( lib, "userenv.lib")
#endif

lws_protocols __defaultProtocols[2] = { 0 };

#define LOG(x) do{ std::cout << "$~~~~~~~~~~$: " << x  << std::endl; }while(0)

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t dataLen);

lws_context_creation_info prepare_create_context();

lws* connect(lws_context *ctx, const char *url, const std::vector<std::string>& list);
template<typename ... T>
lws* connect(lws_context *ctx, const char *url, std::vector<std::string> &list, const char *protocal, T... rest);
template<typename ... T>
lws* connect(lws_context *ctx, const char *url, const char *protocal, T... rest);
lws* connect(lws_context *ctx, const char *url);


int main(int argc, char **argv)
{

    uv_loop_t *loop = uv_default_loop();
    
    lws_context_creation_info createInfo = prepare_create_context();
    lws_context *ctx = lws_create_context(&createInfo);

#if USE_LIBUV
    lws_uv_initloop(ctx, loop, 0);
#endif 

    lws *wsi = connect(ctx, "ws://invalid.url.com/", "invalid_protocol");
    
#if USE_LIBUV
    lws_uv_initloop(ctx, loop, 0);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
#else

    while (true)
    {
        lws_service(ctx, 2);
    }
#endif


    return 0;
}

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t dataLen)
{
    int ret = 0;
    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LOG("connected");
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG("connect error");
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        LOG("wsi destroy");
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
        LOG("client received");
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        LOG("client writable");
        break;
    case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
    case LWS_CALLBACK_LOCK_POLL:
    case LWS_CALLBACK_UNLOCK_POLL:
        break;
    default:
        LOG("unhandled event " << reason);
        break;
    }
    return ret;
}

lws_context_creation_info prepare_create_context()
{
    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    //setup protocols
    memset(&__defaultProtocols, 0, 2 * sizeof(lws_protocols));
    __defaultProtocols[0].name = "";
    __defaultProtocols[0].callback = websocket_callback;
    __defaultProtocols[0].rx_buffer_size = 4 * 1024; //4K buffer
    __defaultProtocols[0].id = -1;
    //setup info
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = __defaultProtocols;
    info.gid = -1;
    info.uid = -1;

    info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;// | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#if USE_LIBUV
    info.options |= LWS_SERVER_OPTION_LIBUV;
#endif

    info.user = nullptr; //redundant
    return info;
}

lws* connect(lws_context *ctx, const char *url, const std::vector<std::string>& list)
{
    lws_client_connect_info clientInfo;
    struct lws_vhost *host;
    static lws_extension exts[] = {
        { "permessage-deflate", lws_extension_callback_pm_deflate, "permessage-deflate; client_max_window_bits" },
    { "deflate-frame", lws_extension_callback_pm_deflate, "deflate_frame" },
    { nullptr, nullptr, nullptr }
    };

    bool useSSL = false;
    int port = 80;
    Url uri(url);
    if (uri.scheme() == "wss" || uri.scheme() == "https")
    {
        useSSL = true;
    }

    if (uri.port().empty())
    {
        port = useSSL ? 443 : 80;
    }
    else
    {
        port = atoi(uri.port().c_str());
    }

    lws_protocols *protocols = nullptr;
    std::string supportedProtocols = "";
    if (list.size() > 0)
    {
        protocols = (lws_protocols*)calloc(list.size() + 1, sizeof(lws_protocols));
        for (int i = 0; i < list.size(); i++)
        {
            lws_protocols *p = protocols + i;
            p->callback = websocket_callback;
            p->name = strdup(list[i].c_str());
            p->id = i;
            supportedProtocols += list[i];
            if (i < list.size() - 1)
            {
                supportedProtocols += ",";
            }
        }
    }
    else {
        protocols = __defaultProtocols;
    }

    lws_context_creation_info ctxInfo;
    memset(&ctxInfo, 0, sizeof(ctxInfo));
    ctxInfo.port = CONTEXT_PORT_NO_LISTEN;
    ctxInfo.uid = -1;
    ctxInfo.gid = -1;
    ctxInfo.protocols = protocols;
    if (useSSL)
        ctxInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
#if USE_LIBUV
    ctxInfo.options |= LWS_SERVER_OPTION_LIBUV;
#endif

    host = lws_create_vhost(ctx, &ctxInfo);

    memset(&clientInfo, 0, sizeof(clientInfo));
    clientInfo.context = ctx;
    clientInfo.address = uri.host().c_str();
    clientInfo.port = port;
    clientInfo.ssl_connection = useSSL ? (LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK) : 0;
    clientInfo.path = uri.path().c_str();
    clientInfo.host = uri.host().c_str();
    clientInfo.origin = uri.host().c_str();
    clientInfo.protocol = supportedProtocols.c_str();
    clientInfo.ietf_version_or_minus_one = -1;
    clientInfo.userdata = nullptr;
    clientInfo.client_exts = exts;
    clientInfo.vhost = host;

    lws *instance = lws_client_connect_via_info(&clientInfo);
    if (instance == nullptr)
    {
        std::cerr << "lws_client_connect_via_info(): " << "" << std::endl;
    }

    return instance;
}








////////////////-------------------begin--------------------///////////////
//template 1
template<typename ... T>
lws* connect(lws_context *ctx, const char *url, std::vector<std::string> &list, const char *protocal, T... rest)
{
    list.push_back(protocal);
    return connect(ctx, url, list, rest...);
}
//template 2
template<typename ... T>
lws* connect(lws_context *ctx, const char *url, const char *protocal, T... rest)
{
    std::vector<std::string> list;
    list.push_back(protocal);
    return connect(ctx, url, list, rest...);
}
//template 3
lws* connect(lws_context *ctx, const char *url)
{
    return connect(ctx, url, std::vector<std::string>());
}
////////////////-------------------end--------------------///////////////


