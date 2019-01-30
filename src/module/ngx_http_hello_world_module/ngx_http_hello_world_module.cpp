#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>

using namespace std;

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
}

typedef struct {
    ngx_str_t output_words;
} ngx_http_hello_world_loc_conf_t;
 
// set函数处理HelloWorld模块的配置项的参数
static char* ngx_http_hello_world(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
 
// 创建数据结构用于存储loc级别(location{}块)的配置项
static void* ngx_http_hello_world_create_loc_conf(ngx_conf_t* cf);
 
// 合并srv级别和loc级别下的同名配置项
static char* ngx_http_hello_world_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);
 
/* -D- ********************|__LiuHongWei__2018/10/23__|************************\
commands数组用于定义模块的配置参数, 每一个数组元素都是mgx_command_t类型, 数组的结尾用ngx_null_command表示。
Nginx在解析配置文件中的一个配置项时首先会遍历所有的模块, 对于每一个模块而言, 即通过遍历commands数组进行, 
在数组中检查到ngx_null_command时, 会停止使用当前模块解析该配置项.
mgx_command_t类型:
typedef struct ngx_command_s ngx_command_t;
struct ngx_command_s{
    ngx_str_t       name;           //配置项名字, 如hello_world
    
    //配置项类型, type将指定配置项可以出现的位置, 例如出现在server{}或者location{}, 以及它可以带参数的个数.
    ngx_uint_t      type;    

    //出现name中指定的配置项后, 将调用set函数处理配置项的参数
    char *(* set)(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    ngx_uint_t      conf;           //在配置文件中的偏移
    ngx_uint_t      offset;         //用于预设的解析方法解析配置项
    void*           post;           //配置项读取后的处理方法, 必须是ngx_conf_post_t的结构体指针
}
\******************************************************************************/ 
static ngx_command_t ngx_http_hello_world_commands[] = {
    {
        ngx_string("hello_world"),                  // 配置项名字
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,         //配置项类型及参数个数
        ngx_http_hello_world,                       //set函数处理配置项的参数
        NGX_HTTP_LOC_CONF_OFFSET,                   
        offsetof(ngx_http_hello_world_loc_conf_t, output_words),
        NULL
    },
    ngx_null_command
};
 
/* -D- ********************|__LiuHongWei__2018/10/23__|************************\
上下文结构体：
HTTP框架在读取和重载配置文件时定义了由ngx_http_module_t接口描述的8个阶段, 
HTTP框架在启动过程中会在每个阶段中调用ngx_http_module_t接口中相应的方法.
如果其中的某个回调方法设为NULL空指针, 则HTTP框架不会调用它.
ngx_http_module_t接口:
typedef struct{
    ngx_int_t (* preconfiguration)(ngx_conf_t* cf);                     //解析配置文件前调用
    ngx_int_t (* postconfiguration)(ngx_conf_t* cf);                    //完成配置文件的解析后调用
    
    //当需要创建数据结构用于存储main级别(http{}块)的全局配置项时调用, 创建存储全局配置项的结构体
    void *(* create_main_conf)(ngx_conf_t* cf);         
    char *(* init_main_conf)(ngx_conf_t* cf, void* conf);               //用于初始化main级别(http{}块)的配置项
    
    //同create_main_conf, 处理srv级别(server{}块)的配置项
    void *(* create_srv_conf)(ngx_conf_t* cf); 
    char *(* merge_srv_conf)(ngx_conf_t* cf, void* prev, void* conf);   //合并main级别和srv级别下的同名配置项
    
    //同create_main_conf, 处理loc级别(location{}块)的配置项
    void *(* create_loc_conf)(ngx_conf_t* cf); 
    char *(* merge_loc_conf)(ngx_conf_t* cf, void* prev, void* conf);   //合并srv级别和loc级别下的同名配置项
}ngx_http_module_t;
\******************************************************************************/ 
static ngx_http_module_t ngx_http_hello_world_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_hello_world_create_loc_conf,
    ngx_http_hello_world_merge_loc_conf
};
 
/* -D- ********************|__LiuHongWei__2018/10/23__|************************\
    最重要的是HelloWorld模块的结构体, 使用了上面定义的其他结构体。
    这一模块在编译时将会被加入到ngx_modules全局数组中, Nginx在启动时会调用所有模块的初始化回调方法.
\******************************************************************************/
ngx_module_t ngx_http_hello_world_module = {
    NGX_MODULE_V1,
    &ngx_http_hello_world_module_ctx,       //上下文结构体
    ngx_http_hello_world_commands,          //commands数组
    NGX_HTTP_MODULE,                        //模块类型
    NULL,                                   //init master
    NULL,                                   //init module 
    NULL,                                   //init process
    NULL,                                   //init thread
    NULL,                                   //exit thread 
    NULL,                                   //exit process
    NULL,                                   //exit master
    NGX_MODULE_V1_PADDING
};

/* -F- ********************|__LiuHongWei__2018/10/23__|************************\
--Fun:  在出现HelloWorld模块的配置项时, ngx_http_hello_world方法会被调用, 
        这时将ngx_http_core_loc_conf_t结构的handler成员指定为ngx_http_hello_world_handler, 
        当HTTP框架在接收完HTTP请求的头部后, 会调用handler指向的方法, 即下面的ngx_http_hello_world_handler.
        这个方法的返回值可以是HTTP中响应包中的返回码, 其中包括了HTTP框架在/src/http/ngx_http_request.h文件中定义好的宏.
--Arg:
--Ret: 
\******************************************************************************/ 
static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t* r) {
    ngx_http_hello_world_loc_conf_t* hlcf = (ngx_http_hello_world_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_hello_world_module);
 
    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char*)"text/plain";
 
    //在内存池中分配内存
    ngx_buf_t* b = (ngx_buf_t*)ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
 
    ngx_chain_t out[2];
    out[0].buf = b;
    out[0].next = &out[1];
 
    b->pos = (u_char*)"hello_world, ";
    b->last = b->pos + sizeof("hello_world, ") - 1;
    b->memory = 1;
 
    b = (ngx_buf_t*)ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
 
    out[1].buf = b;
    out[1].next = NULL;
 
    b->pos = hlcf->output_words.data;
    b->last = hlcf->output_words.data + (hlcf->output_words.len);
    b->memory = 1;
    b->last_buf = 1;
 
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = hlcf->output_words.len + sizeof("hello_world, ") - 1;
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    //测试c++代码, for循环的几种写法, 注意只有nginx.conf中配置daemon off master_process off才会在控制台看到输出
    vector<int> vi{3, 5, 1, 4, 2};
    cout << "1. 下标: ";
    for(size_t i = 0; i < vi.size(); i++){
        cout << vi.at(i) << " ";
    }
    cout << endl;
    
    cout << "2. 迭代器: ";
    for(auto it = vi.begin(); it != vi.end(); it++){
        cout << *it << " ";
    }
    cout << endl;

    cout << "3. lambda: ";
    auto lmd = [](int i){
        cout << i << " ";
    };
    for_each(vi.begin(), vi.end(), lmd);
    cout << endl;
    
    cout << "4. 范围for: ";
    for(auto value : vi){
        cout << value << " ";
    }
    cout << endl;
    
    //测试c++代码, 测试输出到文件, 注意只有nginx.conf中配置daemon off master_process off才会看到文件输出
    ofstream o("/test.txt");
    for(size_t i = 0; i < vi.size(); i++){
        o << vi.at(i) << " ";
    }
    o << endl;
    
    return ngx_http_output_filter(r, &out[0]);              //向用户发送响应包.
}

/* -F- ********************|__LiuHongWei__2018/10/23__|************************\
--Fun:  创建数据结构用于存储loc级别(location{}块)的配置项
--Arg:
--Ret: 
\******************************************************************************/ 
static void* ngx_http_hello_world_create_loc_conf(ngx_conf_t* cf) {
    ngx_http_hello_world_loc_conf_t* conf;
 
    conf = (ngx_http_hello_world_loc_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_hello_world_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    conf->output_words.len = 0;
    conf->output_words.data = NULL;
 
    return conf;
}
 
/* -F- ********************|__LiuHongWei__2018/10/23__|************************\
--Fun:  合并srv级别和loc级别下的同名配置项
--Arg:
--Ret: 
\******************************************************************************/
static char* ngx_http_hello_world_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child) {
    ngx_http_hello_world_loc_conf_t* conf = (ngx_http_hello_world_loc_conf_t*)child;
    ngx_http_hello_world_loc_conf_t* prev = (ngx_http_hello_world_loc_conf_t*)parent;
    
    ngx_conf_merge_str_value(conf->output_words, prev->output_words, "Nginx");
    
    return NGX_CONF_OK;
}
 
/* -F- ********************|__LiuHongWei__2018/10/23__|************************\
--Fun:  此函数是mgx_command_t结构体中的set成员, 完整定义是:
        char* （* set)(ngx_conf_t* cmd, void *)
--Arg:
--Ret: 
\******************************************************************************/
static char* ngx_http_hello_world(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    ngx_http_core_loc_conf_t* clcf = (ngx_http_core_loc_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    
    clcf->handler = ngx_http_hello_world_handler; 
    
    ngx_conf_set_str_slot(cf, cmd, conf);
    
    return NGX_CONF_OK;
}
