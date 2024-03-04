/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-08-03    chenyong      the first version
 */

#include <string.h>

#include <rtthread.h>
#include <webclient.h>

#define POST_RESP_BUFSZ 1024 * 1024 * 2
#define POST_HEADER_BUFSZ 1024 * 5

#define POST_LOCAL_URI "https://wx.lstabc.com/weixin"

const char *post_data = "<xml><ToUserName><![CDATA[artpi]]></ToUserName><FromUserName><![CDATA[artpi]]></FromUserName><CreateTime>1348831860</CreateTime><MsgType><![CDATA[text]]></MsgType><Content><![CDATA[你好]]></Content><MsgId>123</MsgId></xml>";
#define BUILD_MESSAGE "<xml><ToUserName><![CDATA[artpi]]></ToUserName><FromUserName><![CDATA[artpi]]></FromUserName><CreateTime>1348831860</CreateTime><MsgType><![CDATA[text]]></MsgType><Content><![CDATA[%s]]></Content><MsgId>123</MsgId></xml>"

// date 2024 02 27 19 35 30
/* send HTTP POST request by common request interface, it used to receive longer data */
static int webclient_post_comm(const char *uri, const void *post_data, size_t data_len)
{
    struct webclient_session *session = RT_NULL;
    unsigned char *buffer = RT_NULL;
    int index, ret = 0;
    int bytes_read, resp_status;

    buffer = (unsigned char *)web_malloc(POST_RESP_BUFSZ);
    if (buffer == RT_NULL)
    {
        rt_kprintf("no memory for receive response buffer.\n");
        ret = -RT_ENOMEM;
        goto __exit;
    }

    /* create webclient session and set header response size */
    session = webclient_session_create(POST_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        ret = -RT_ENOMEM;
        goto __exit;
    }

    /* build header for upload */
    webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/octet-stream\r\n");

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200)
    {
        rt_kprintf("webclient POST request failed, response(%d) error.\n", resp_status);
        ret = -RT_ERROR;
        goto __exit;
    }

    rt_kprintf("webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, POST_RESP_BUFSZ);
        if (bytes_read <= 0)
        {
            break;
        }

        for (index = 0; index < bytes_read; index++)
        {
            rt_kprintf("%c", buffer[index]);
        }
    } while (1);

    rt_kprintf("\n");

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    return ret;
}

/* send HTTP POST request by simplify request interface, it used to received shorter data */
static int webclient_post_smpl(const char *uri, const char *post_data, size_t data_len)
{
    char *response = RT_NULL;
    char *header = RT_NULL;
    size_t resp_len = 0;
    int index = 0;

    webclient_request_header_add(&header, "Content-Length: %d\r\n", strlen(post_data));
    webclient_request_header_add(&header, "Content-Type: application/octet-stream\r\n");

    if (webclient_request(uri, header, post_data, data_len, (void **)&response, &resp_len) < 0)
    {
        rt_kprintf("webclient send post request failed.");
        web_free(header);
        return -RT_ERROR;
    }

    rt_kprintf("webclient send post request by simplify request interface.\n");
    rt_kprintf("webclient post response data: \n");
    for (index = 0; index < resp_len; index++)
    {
        rt_kprintf("%c", response[index]);
    }
    rt_kprintf("\n");

    if (header)
    {
        web_free(header);
    }

    if (response)
    {
        web_free(response);
    }

    return 0;
}


//  web_post_test 鲁迅和周树人是啥关系
//
// "<xml><ToUserName><![CDATA[artpi]]></ToUserName><FromUserName><![CDATA[artpi]]></FromUserName><CreateTime>1348831860</CreateTime><MsgType><![CDATA[text]]></MsgType><Content><![CDATA[你好]]></Content><MsgId>123</MsgId></xml>"
int webclient_post_test(int argc, char **argv)
{
    char *uri = RT_NULL;
    char *data = RT_NULL;
    int message_length = 0;

    // web_post_test post_data
    if (argc == 1)
    {
        uri = web_strdup(POST_LOCAL_URI);
        if (uri == RT_NULL)
        {
            rt_kprintf("no memory for create post request uri buffer.\n");
            return -RT_ENOMEM;
        }
        webclient_post_comm(uri, (void *)post_data, rt_strlen(post_data));
    }
    else if (argc == 2) {
        uri = web_strdup(POST_LOCAL_URI);
        if (uri == RT_NULL)
        {
            rt_kprintf("no memory for create post request uri buffer.\n");
            return -RT_ENOMEM;
        }

        message_length = strlen(argv[1]) + 300;
        data = web_malloc(message_length);
        if (data == RT_NULL)
        {
            rt_kprintf("no memory for create post data buffer.\n");
            return -RT_ENOMEM;
        }
        rt_sprintf(data, BUILD_MESSAGE, argv[1]);
        rt_kprintf("data len: %d\n", rt_strlen(data));
            // rt_strncpy(data, argv[1], strlen(argv[1]));
        webclient_post_comm(uri, (void *)data, rt_strlen(data));
        // webclient_post_comm(uri, (void *)post_data, rt_strlen(post_data));
    }
    else
    {
        rt_kprintf("wrong.\n");
    }

#if 0
    else if (argc == 2)
    {
        if (rt_strcmp(argv[1], "-s") == 0)
        {
            uri = web_strdup(POST_LOCAL_URI);
            if(uri == RT_NULL)
            {
                rt_kprintf("no memory for create post request uri buffer.\n");
                return -RT_ENOMEM;
            }

            webclient_post_smpl(uri, (void *)post_data, rt_strlen(post_data));
        }
        else
        {
            uri = web_strdup(argv[1]);
            if(uri == RT_NULL)
            {
                rt_kprintf("no memory for create post request uri buffer.\n");
                return -RT_ENOMEM;
            }
            webclient_post_comm(uri, (void *)post_data, rt_strlen(post_data));
        }
    }
    else if(argc == 3 && rt_strcmp(argv[1], "-s") == 0)
    {
        uri = web_strdup(argv[2]);
        if(uri == RT_NULL)
        {
            rt_kprintf("no memory for create post request uri buffer.\n");
            return -RT_ENOMEM;
        }

        webclient_post_smpl(uri, (void *)post_data, rt_strlen(post_data));
    }
    else
    {
        rt_kprintf("web_post_test [uri]     - webclient post request test.\n");
        rt_kprintf("web_post_test -s [uri]  - webclient simplify post request test.\n");
        return -RT_ERROR;
    }
#endif

    if (uri)
    {
        web_free(uri);
    }

    if (data)
    {
        web_free(data);
    }

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(webclient_post_test, web_post_test, webclient post request test.);
#endif /* FINSH_USING_MSH */
