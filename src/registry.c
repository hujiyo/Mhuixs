#include "registry.h"
#include "tblh.h"
#include "kvalh.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
/* 保存SID宏定义,包含windows.h后恢复 */
#pragma push_macro("SID")
#undef SID
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#pragma pop_macro("SID")
#endif

/* 全局注册表实例 */
Registry Reg;

/* 初始化注册表 */
int reg_init(void) {
    Reg.hook_map = hash_create(1024); /* 初始容量 1024，适合大规模数据 */
    if (!Reg.hook_map) {
        return -1;
    }
    
#ifdef _WIN32
    Reg.lock = malloc(sizeof(CRITICAL_SECTION));
    if (!Reg.lock) {
        hash_destroy(Reg.hook_map, NULL);
        return -1;
    }
    InitializeCriticalSection((CRITICAL_SECTION*)Reg.lock);
#else
    if (pthread_mutex_init(&Reg.lock, NULL) != 0) {
        hash_destroy(Reg.hook_map, NULL);
        return -1;
    }
#endif
    
    return 0;
}

/* 销毁注册表 */
void reg_destroy(void) {
    if (Reg.hook_map) {
        hash_destroy(Reg.hook_map, NULL); /* 不释放 HOOK*，由外部管理 */
        Reg.hook_map = NULL;
    }
    
#ifdef _WIN32
    if (Reg.lock) {
        DeleteCriticalSection((CRITICAL_SECTION*)Reg.lock);
        free(Reg.lock);
        Reg.lock = NULL;
    }
#else
    pthread_mutex_destroy(&Reg.lock);
#endif
}

/* 加锁 */
static inline void reg_lock(void) {
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION*)Reg.lock);
#else
    pthread_mutex_lock(&Reg.lock);
#endif
}

/* 解锁 */
static inline void reg_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION*)Reg.lock);
#else
    pthread_mutex_unlock(&Reg.lock);
#endif
}

/* 注册HOOK */
int reg_register_hook(M_UID owner, const char* name, HOOK** hook_return) {
    if (!hook_return) return -2; /* 空HOOK指针，注册失败 */
    if (!name || strlen(name) == 0) return -1; /* 空名字，注册失败 */

    /* 先查重名：这一步必须在创建 HOOK 之前。
     * 否则重名时若调用 HOOK_logout()，它会按 hook->name 去注册表里注销，
     * 而新建的同名 HOOK 与已存在的那个同名 —— 结果是"重名保护"把原数据删掉。
     */
    reg_lock();
    if (hash_contains(Reg.hook_map, name)) {
        reg_unlock();
        return 1; /* 已有同名HOOK，注册失败 */
    }
    reg_unlock();

    /* 创建 mstring */
    mstring mname = mstr(name);
    if (!mname) return -1;

    /* 创建 HOOK */
    HOOK* hook = HOOK_login(owner, mname, NULL);
    if (!hook) {
        mstr_free(mname);
        return -1;
    }

    /* 注册HOOK */
    reg_lock();

    /* 双检：并发情况下同名 HOOK 仍可能在这中间被插入 */
    if (hash_contains(Reg.hook_map, name)) {
        reg_unlock();
        /* 此处不能调用 HOOK_logout（它会按名字注销掉别人），
         * 只需释放这个尚未入表的 HOOK 自身。 */
        mstr_free(hook->name);
        free(hook);
        return 1;
    }

    /* 添加到哈希表 */
    if (hash_put(Reg.hook_map, name, hook) != 0) {
        reg_unlock();
        mstr_free(hook->name);
        free(hook);
        return -1;
    }

    reg_unlock();

    *hook_return = hook;
    return 0;
}

/* 注销HOOK */
void reg_unregister_hook(const char* name) {
    if (!name) return;
    
    reg_lock();
    
    HOOK* hook = (HOOK*)hash_get(Reg.hook_map, name);
    if (hook) {
        hash_remove(Reg.hook_map, name);
    }
    
    reg_unlock();
}

/* 查找HOOK */
HOOK* reg_find_hook(const char* name) {
    if (!name) return NULL;
    
    reg_lock();
    HOOK* hook = (HOOK*)hash_get(Reg.hook_map, name);
    reg_unlock();
    
    return hook;
}

/* 判断HOOK是否已注册 */
int reg_is_registered(const char* name) {
    if (!name) return 0;
    
    reg_lock();
    int exists = hash_contains(Reg.hook_map, name);
    reg_unlock();
    
    return exists;
}

/* ==================== C 接口层实现 ==================== */

int reg_register(uint64_t owner, const char *name, HookHandle *out_hook) {
    if (!name || !out_hook) return -2;
    
    HOOK *hook = NULL;
    int ret = reg_register_hook(owner, name, &hook);
    *out_hook = (HookHandle)hook;
    return ret;
}

void reg_unregister(const char *name) {
    if (!name) return;
    reg_unregister_hook(name);
}

HookHandle reg_find(const char *name) {
    if (!name) return NULL;
    return (HookHandle)reg_find_hook(name);
}

int hook_reg_exists(const char *name) {
    if (!name) return 0;
    return reg_is_registered(name);
}

int hook_new(HookHandle hook, uint64_t caller, int objtype, void *p1, void *p2, void *p3) {
    if (!hook) return -1;
    HOOK *h = (HOOK*)hook;
    return hook_new_obj(h, caller, (obj_type)objtype, p1, p2, p3);
}

int hook_set_permission(HookHandle hook, uint64_t caller, const char *pm_str) {
    if (!hook || !pm_str) return -1;
    HOOK *h = (HOOK*)hook;
    hook_reset_pm(h, caller, pm_str);
    return 0;
}

/* ==================== BHS 深拷贝 ==================== */

BHS* bhs_deep_copy(const BHS *src) {
    if (!src) return NULL;
    
    BHS *dst = (BHS*)calloc(1, sizeof(BHS));
    if (!dst) return NULL;
    
    bignum_init(dst);
    if (bignum_copy(src, dst) != 0) {
        free(dst);
        return NULL;
    }
    return dst;
}

int hook_set_bhs(HookHandle hook, uint64_t caller, void *bhs) {
    if (!hook || !bhs) return -1;
    HOOK *h = (HOOK*)hook;
    
    /* 权限检查:需要ADD和CHANGE权限 */
    if (caller != 0) {
        if (!is_entitled(h, caller, HOOK_ADD) ||
            !is_entitled(h, caller, HOOK_CHANGE)) {
            return -2; /* 权限不足 */
        }
    }
    
    /* 释放旧的BHS（如果存在） */
    if (h->obj) {
        bignum_destroy(h->obj);
        h->obj = NULL;
    }
    
    /* 深拷贝BHS，避免外部释放后指针失效 */
    BHS *copy = bhs_deep_copy((const BHS*)bhs);
    if (!copy) return -1;
    
    h->obj = copy;
    return 0;
}

void* hook_get_bhs(HookHandle hook, uint64_t caller) {
    if (!hook) return NULL;
    HOOK *h = (HOOK*)hook;
    
    /* 权限检查:需要READ权限 */
    if (caller != 0) {
        if (!is_entitled(h, caller, HOOK_READ)) {
            return NULL; /* 权限不足 */
        }
    }
    
    /* 返回 HOOK 内部 BHS 的指针 */
    return (void*)h->obj;
}

/* ==================== 持久化功能 ==================== */

/* 获取注册表中HOOK的数量 */
int reg_get_hook_count(void) {
    if (!Reg.hook_map) return 0;
    return hash_size(Reg.hook_map);
}

/* ==================== BHS 序列化/反序列化 ==================== */

/*
 * BHS 二进制序列化格式:
 * [type:       int32]    类型标记
 * [is_large:   int32]    是否大数据
 * [length:     uint32]   数据长度
 * [type_data:  8 bytes]  类型特定数据（decimal_pos + is_negative 等）
 * [has_data:   uint8]    是否有数据内容
 * [data_size:  uint32]   数据字节数（仅当 has_data=1）
 * [data:       N bytes]  数据内容（仅当 has_data=1）
 *
 * LIST 类型特殊处理:
 * [list_count: uint32]   列表元素数量
 * [elements:   N * BHS]  递归序列化每个元素
 */

/* ==================== 序列化辅助 ==================== */

/* 写一个带 uint32 长度前缀的字节串 */
static int write_bytes32(const char *data, uint32_t len, FILE *fp) {
    if (fwrite(&len, sizeof(uint32_t), 1, fp) != 1) return -1;
    if (len > 0) {
        if (!data) return -1;
        if (fwrite(data, 1, len, fp) != len) return -1;
    }
    return 0;
}

/* 读一个带 uint32 长度前缀的字节串；成功返回 0 并把 malloc 的缓冲区放到 *out */
static int read_bytes32(char **out, FILE *fp) {
    uint32_t len = 0;
    *out = NULL;
    if (fread(&len, sizeof(uint32_t), 1, fp) != 1) return -1;
    if (len > (64u * 1024u * 1024u)) return -1;   /* 上限，防损坏文件撑爆内存 */
    if (len == 0) return 0;
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) return -1;
    if (fread(buf, 1, len, fp) != len) { free(buf); return -1; }
    buf[len] = '\0';
    *out = buf;
    return 0;
}

/*
单元格读写：先写 1 字节 has_value，再写 BHS。
bhs_deserialize 对"空 BHS"和"读取失败"都返回 NULL，无法区分，
所以这里额外加一个标志字节，把"空单元格"和"文件损坏"分开。
*/
static int write_cell(const BHS *v, FILE *fp) {
    uint8_t has = (v != NULL) ? 1 : 0;
    if (fwrite(&has, 1, 1, fp) != 1) return -1;
    if (!has) return 0;
    return bhs_serialize(v, fp);
}

static BHS* read_cell(FILE *fp, int *err) {
    uint8_t has = 0;
    *err = 0;
    if (fread(&has, 1, 1, fp) != 1) { *err = 1; return NULL; }
    if (!has) return NULL;              /* 空单元格，不是错误 */
    BHS *v = bhs_deserialize(fp);
    if (!v) *err = 1;                   /* 说好了有值却读不出来 = 损坏 */
    return v;
}

/* ---- TABLE ---- */

static int table_serialize(const TABLE *t, FILE *fp) {
    if (!t) return -1;

    if (t->name) {
        if (write_bytes32(mstr_cstr(t->name), (uint32_t)mstrlen(t->name), fp) != 0) return -1;
    } else {
        if (write_bytes32(NULL, 0, fp) != 0) return -1;
    }

    uint32_t fn = (uint32_t)t->field_num;
    if (fwrite(&fn, sizeof(uint32_t), 1, fp) != 1) return -1;

    for (uint32_t i = 0; i < fn; i++) {
        const FIELD *f = &t->field[i];
        if (f->name) {
            if (write_bytes32(mstr_cstr(f->name), (uint32_t)mstrlen(f->name), fp) != 0) return -1;
        } else {
            if (write_bytes32(NULL, 0, fp) != 0) return -1;
        }
        int32_t ty = (int32_t)f->type;
        if (fwrite(&ty, sizeof(int32_t), 1, fp) != 1) return -1;
    }

    /* 只保存逻辑行（按逻辑顺序）；容量在重建时由 create_table 决定 */
    uint32_t rows = (uint32_t)get_record_count((TABLE*)t);
    if (fwrite(&rows, sizeof(uint32_t), 1, fp) != 1) return -1;

    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < fn; c++) {
            if (write_cell(get_value((TABLE*)t, r, c), fp) != 0) return -1;
        }
    }
    return 0;
}

static TABLE* table_deserialize(FILE *fp) {
    char *tname_buf = NULL;
    if (read_bytes32(&tname_buf, fp) != 0) return NULL;

    uint32_t fn = 0;
    if (fread(&fn, sizeof(uint32_t), 1, fp) != 1) { free(tname_buf); return NULL; }
    if (fn == 0 || fn > 100000u) { free(tname_buf); return NULL; }

    int     *types = (int*)calloc(fn, sizeof(int));
    mstring *names = (mstring*)calloc(fn, sizeof(mstring));
    if (!types || !names) { free(tname_buf); free(types); free(names); return NULL; }

    int ok = 1;
    for (uint32_t i = 0; i < fn && ok; i++) {
        char *nb = NULL;
        if (read_bytes32(&nb, fp) != 0) { ok = 0; break; }
        int32_t ty = 0;
        if (fread(&ty, sizeof(int32_t), 1, fp) != 1) { free(nb); ok = 0; break; }
        names[i] = mstr(nb ? nb : (char*)"");
        free(nb);                    /* mstr 已复制内容 */
        if (!names[i]) { ok = 0; break; }
        types[i] = ty;
    }
    if (!ok) {
        for (uint32_t i = 0; i < fn; i++) if (names[i]) mstr_free(names[i]);
        free(types); free(names); free(tname_buf);
        return NULL;
    }

    mstring tname = mstr(tname_buf ? tname_buf : (char*)"");
    free(tname_buf);
    if (!tname) {
        for (uint32_t i = 0; i < fn; i++) mstr_free(names[i]);
        free(types); free(names);
        return NULL;
    }

    /* create_table 接管 names[i] 与 tname 的所有权 */
    TABLE *t = create_table(types, names, fn, tname);
    free(types); free(names);
    if (!t) return NULL;

    uint32_t rows = 0;
    if (fread(&rows, sizeof(uint32_t), 1, fp) != 1) { free_table(t); return NULL; }
    if (rows > 100000000u) { free_table(t); return NULL; }

    for (uint32_t r = 0; r < rows; r++) {
        Obj *vals = (Obj*)calloc(fn, sizeof(Obj));
        if (!vals) { free_table(t); return NULL; }

        int err = 0;
        for (uint32_t c = 0; c < fn; c++) {
            vals[c] = read_cell(fp, &err);
            if (err) {
                for (uint32_t j = 0; j < c; j++) if (vals[j]) bignum_destroy(vals[j]);
                free(vals); free_table(t);
                return NULL;
            }
        }
        /* add_record 接管 vals 里每个非 NULL 元素的所有权 */
        if (add_record(t, vals, fn) != 0) {
            for (uint32_t c = 0; c < fn; c++) if (vals[c]) bignum_destroy(vals[c]);
            free(vals); free_table(t);
            return NULL;
        }
        free(vals);
    }
    return t;
}

/* ---- KVALOT ---- */

static int kvalot_serialize(const KVALOT *kv, FILE *fp) {
    if (!kv) return -1;

    /* 名字本身是一个 BHS（字符串类型） */
    if (bhs_serialize(kv->name, fp) != 0) return -1;

    uint32_t n = kv->num_keys;
    if (fwrite(&n, sizeof(uint32_t), 1, fp) != 1) return -1;

    for (uint32_t i = 0; i < n; i++) {
        const KVPAIR *p = &kv->keypool[i];
        if (p->key) {
            if (write_bytes32(mstr_cstr(p->key), (uint32_t)mstrlen(p->key), fp) != 0) return -1;
        } else {
            if (write_bytes32(NULL, 0, fp) != 0) return -1;
        }
        if (write_cell(p->value, fp) != 0) return -1;
    }
    return 0;
}

static KVALOT* kvalot_deserialize(FILE *fp) {
    BHS *name = bhs_deserialize(fp);
    if (!name) return NULL;
    if (name->type != BIGNUM_TYPE_STRING) { bignum_destroy(name); return NULL; }

    KVALOT *kv = kvalot_create(name);
    bignum_destroy(name);              /* kvalot_create 内部已复制名字 */
    if (!kv) return NULL;

    uint32_t n = 0;
    if (fread(&n, sizeof(uint32_t), 1, fp) != 1) { kvalot_destroy(kv); return NULL; }
    if (n > 100000000u) { kvalot_destroy(kv); return NULL; }

    for (uint32_t i = 0; i < n; i++) {
        char *kb = NULL;
        if (read_bytes32(&kb, fp) != 0 || kb == NULL) {
            free(kb); kvalot_destroy(kv); return NULL;
        }

        BHS *key = bignum_from_raw_string(kb);
        free(kb);
        if (!key) { kvalot_destroy(kv); return NULL; }

        int err = 0;
        BHS *val = read_cell(fp, &err);
        if (err || val == NULL) {
            bignum_destroy(key);
            if (val) bignum_destroy(val);
            kvalot_destroy(kv);
            return NULL;
        }

        if (kvalot_add(kv, key, val) != 0) {
            bignum_destroy(key);
            bignum_destroy(val);
            kvalot_destroy(kv);
            return NULL;
        }
        bignum_destroy(key);           /* 键不被接管，值被接管 */
    }
    return kv;
}

/* ==================== BHS 序列化 ==================== */

int bhs_serialize(const BHS *bhs, FILE *fp) {
    if (!fp) return -1;
    
    /* 处理空 BHS */
    uint8_t is_null = (bhs == NULL) ? 1 : 0;
    fwrite(&is_null, sizeof(uint8_t), 1, fp);
    if (is_null) return 0;
    
    /* 写入基本字段 */
    int32_t type = (int32_t)bhs->type;
    fwrite(&type, sizeof(int32_t), 1, fp);
    
    uint32_t length = (uint32_t)bhs->length;
    fwrite(&length, sizeof(uint32_t), 1, fp);
    
    /* 写入类型特定数据 */
    fwrite(&bhs->type_data, 8, 1, fp);
    
    /* 根据类型序列化数据 */
    if (type == BIGNUM_TYPE_LIST) {
        /* LIST 类型：递归序列化每个元素 */
        LIST *list = bhs->data.list;
        uint32_t list_count = list ? (uint32_t)list_size(list) : 0;
        fwrite(&list_count, sizeof(uint32_t), 1, fp);
        
        for (uint32_t i = 0; i < list_count; i++) {
            Obj elem = list_get_index(list, i);
            if (bhs_serialize((const BHS*)elem, fp) != 0) {
                return -1;
            }
        }
    } else if (type == BIGNUM_TYPE_TABLE) {
        /* 表类型：整张表（字段 + 逻辑行 + 每个单元格） */
        if (table_serialize(bhs->data.table, fp) != 0) return -1;
    } else if (type == BIGNUM_TYPE_KVALOT) {
        /* 键值对类型：名字 + 所有键值对 */
        if (kvalot_serialize(bhs->data.kvalot, fp) != 0) return -1;
    } else if (type == BIGNUM_TYPE_NULL) {
        /* NULL 类型无数据 */
    } else if (type == BIGNUM_TYPE_NUMBER || type == BIGNUM_TYPE_STRING ||
               type == BIGNUM_TYPE_BITMAP) {
        /* NUMBER / STRING / BITMAP：序列化原始数据 */
        const char *data_ptr = BIGNUM_DIGITS(bhs);
        uint32_t data_size = (uint32_t)bhs->length;
        
        /* bitmap 类型的 length 是位数，转换为字节数 */
        if (type == BIGNUM_TYPE_BITMAP) {
            data_size = (data_size + 7) / 8;
        }
        
        fwrite(&data_size, sizeof(uint32_t), 1, fp);
        if (data_size > 0 && data_ptr) {
            fwrite(data_ptr, 1, data_size, fp);
        }
    } else {
        /*
         * 未知类型：宁可失败，也不要继续。
         * 旧实现这里是个兜底 else，对 TABLE/KVALOT 会把联合体里
         * 指针的字节当成数据写进磁盘 —— 读回来就是一个
         * "声称是 TABLE、实际指针是垃圾"的 BHS，一碰就崩。
         */
        report(error, "Registry", "bhs_serialize: unsupported BHS type");
        return -1;
    }
    
    return 0;
}

BHS* bhs_deserialize(FILE *fp) {
    if (!fp) return NULL;
    
    /* 检查是否为空 */
    uint8_t is_null;
    if (fread(&is_null, sizeof(uint8_t), 1, fp) != 1) return NULL;
    if (is_null) return NULL;
    
    /* 读取基本字段 */
    int32_t type;
    uint32_t length;
    if (fread(&type, sizeof(int32_t), 1, fp) != 1) return NULL;
    if (fread(&length, sizeof(uint32_t), 1, fp) != 1) return NULL;
    
    /* 读取类型特定数据 */
    char type_data_buf[8];
    if (fread(type_data_buf, 8, 1, fp) != 1) return NULL;
    
    if (type == BIGNUM_TYPE_LIST) {
        /* LIST 类型：递归反序列化 */
        uint32_t list_count;
        if (fread(&list_count, sizeof(uint32_t), 1, fp) != 1) return NULL;
        
        BHS *result = bignum_create_list();
        if (!result) return NULL;
        
        LIST *list = bignum_get_list(result);
        if (!list && list_count > 0) {
            bignum_destroy(result);
            return NULL;
        }
        
        for (uint32_t i = 0; i < list_count; i++) {
            BHS *elem = bhs_deserialize(fp);
            if (!elem) {
                bignum_destroy(result);
                return NULL;
            }
            /* list_rpush 接管 elem 的所有权；失败必须自己销毁，否则泄漏 */
            if (list_rpush(list, elem) != 0) {
                bignum_destroy(elem);
                bignum_destroy(result);
                return NULL;
            }
        }
        
        result->length = length;
        memcpy(&result->type_data, type_data_buf, 8);
        return result;
        
    } else if (type == BIGNUM_TYPE_TABLE) {
        /* 表类型：直接接管反序列化出来的 TABLE，避免多一次深拷贝 */
        TABLE *t = table_deserialize(fp);
        if (!t) return NULL;

        BHS *result = bignum_create();
        if (!result) { free_table(t); return NULL; }

        result->type = BIGNUM_TYPE_TABLE;
        result->data.table = t;
        result->is_large = 0;
        result->capacity = 0;
        result->length = length;
        memcpy(&result->type_data, type_data_buf, 8);
        return result;

    } else if (type == BIGNUM_TYPE_KVALOT) {
        KVALOT *kv = kvalot_deserialize(fp);
        if (!kv) return NULL;

        BHS *result = bignum_create();
        if (!result) { kvalot_destroy(kv); return NULL; }

        result->type = BIGNUM_TYPE_KVALOT;
        result->data.kvalot = kv;
        result->is_large = 0;
        result->capacity = 0;
        result->length = length;
        memcpy(&result->type_data, type_data_buf, 8);
        return result;

    } else if (type == BIGNUM_TYPE_NULL) {
        /* NULL 类型 */
        BHS *result = bignum_create();
        if (!result) return NULL;
        result->type = BIGNUM_TYPE_NULL;
        result->length = 0;
        memcpy(&result->type_data, type_data_buf, 8);
        return result;
        
    } else if (type == BIGNUM_TYPE_NUMBER || type == BIGNUM_TYPE_STRING ||
               type == BIGNUM_TYPE_BITMAP) {
        /* NUMBER / STRING / BITMAP */
        uint32_t data_size;
        if (fread(&data_size, sizeof(uint32_t), 1, fp) != 1) return NULL;
        
        BHS *result = bignum_create();
        if (!result) return NULL;
        
        result->type = type;
        result->length = length;
        memcpy(&result->type_data, type_data_buf, 8);
        
        if (data_size > 0) {
            if (data_size <= BIGNUM_SMALL_SIZE) {
                result->is_large = 0;
                result->capacity = BIGNUM_SMALL_SIZE;
                if (fread(result->data.small_data, 1, data_size, fp) != data_size) {
                    bignum_destroy(result);
                    return NULL;
                }
            } else {
                result->is_large = 1;
                result->capacity = data_size;
                result->data.large_data = (char*)malloc(data_size);
                if (!result->data.large_data) {
                    bignum_destroy(result);
                    return NULL;
                }
                if (fread(result->data.large_data, 1, data_size, fp) != data_size) {
                    bignum_destroy(result);
                    return NULL;
                }
            }
        }
        
        return result;
    }

    /* 未知类型：明确失败，不要返回一个 type 与实际数据不匹配的 BHS */
    report(error, "Registry", "bhs_deserialize: unsupported BHS type");
    return NULL;
}

/* ==================== HOOK 序列化/反序列化 ==================== */

/*
 * HOOK 二进制序列化格式:
 * [name_len:    uint32]          名称长度
 * [name:        name_len bytes]  名称内容
 * [owner:       uint64]          所有者UID
 * [group:       int64]           组GID
 * [permissions: 10 bytes]        权限结构体(ifisinit + 9个权限位)
 * [has_bhs:     uint8]           是否有BHS数据
 * [bhs_data:    ...]             BHS序列化数据（仅当 has_bhs=1）
 */

static int hook_serialize(const char *name, HOOK *hook, FILE *fp) {
    if (!name || !hook || !fp) return -1;
    
    /* 写入名称 */
    uint32_t name_len = (uint32_t)strlen(name);
    fwrite(&name_len, sizeof(uint32_t), 1, fp);
    fwrite(name, 1, name_len, fp);
    
    /* 写入 owner 和 group */
    uint64_t owner = (uint64_t)hook->owner;
    int64_t group = (int64_t)hook->group;
    fwrite(&owner, sizeof(uint64_t), 1, fp);
    fwrite(&group, sizeof(int64_t), 1, fp);
    
    /* 写入权限结构体（逐字段写入，避免对齐问题） */
    permission_struct *pm = &hook->pm_s;
    uint8_t pm_bytes[10];
    pm_bytes[0] = (uint8_t)pm->ifisinit;
    pm_bytes[1] = (uint8_t)pm->owner_read;
    pm_bytes[2] = (uint8_t)pm->owner_add;
    pm_bytes[3] = (uint8_t)pm->owner_change;
    pm_bytes[4] = (uint8_t)pm->group_read;
    pm_bytes[5] = (uint8_t)pm->group_add;
    pm_bytes[6] = (uint8_t)pm->group_change;
    pm_bytes[7] = (uint8_t)pm->other_read;
    pm_bytes[8] = (uint8_t)pm->other_add;
    pm_bytes[9] = (uint8_t)pm->other_change;
    fwrite(pm_bytes, 1, 10, fp);
    
    /* 写入 BHS 数据 */
    uint8_t has_bhs = (hook->obj != NULL) ? 1 : 0;
    fwrite(&has_bhs, sizeof(uint8_t), 1, fp);
    
    if (has_bhs) {
        if (bhs_serialize((const BHS*)hook->obj, fp) != 0) {
            return -1;
        }
    }
    
    return 0;
}

static HOOK* hook_deserialize(FILE *fp, char **out_name) {
    if (!fp || !out_name) return NULL;
    
    /* 读取名称 */
    uint32_t name_len;
    if (fread(&name_len, sizeof(uint32_t), 1, fp) != 1) return NULL;
    if (name_len == 0 || name_len > 65535) return NULL;
    
    char *name = (char*)malloc(name_len + 1);
    if (!name) return NULL;
    if (fread(name, 1, name_len, fp) != name_len) {
        free(name);
        return NULL;
    }
    name[name_len] = '\0';
    
    /* 读取 owner 和 group */
    uint64_t owner;
    int64_t group;
    if (fread(&owner, sizeof(uint64_t), 1, fp) != 1 ||
        fread(&group, sizeof(int64_t), 1, fp) != 1) {
        free(name);
        return NULL;
    }
    
    /* 读取权限 */
    uint8_t pm_bytes[10];
    if (fread(pm_bytes, 1, 10, fp) != 10) {
        free(name);
        return NULL;
    }
    
    /* 读取 BHS */
    uint8_t has_bhs;
    if (fread(&has_bhs, sizeof(uint8_t), 1, fp) != 1) {
        free(name);
        return NULL;
    }
    
    BHS *bhs_data = NULL;
    if (has_bhs) {
        bhs_data = bhs_deserialize(fp);
        /* bhs_data 可以为 NULL（如果原始数据就是 NULL BHS） */
    }
    
    /* 创建 HOOK */
    mstring mname = mstr(name);
    if (!mname) {
        free(name);
        if (bhs_data) bignum_destroy(bhs_data);
        return NULL;
    }
    
    HOOK *hook = HOOK_login((M_UID)owner, mname, bhs_data);
    if (!hook) {
        mstr_free(mname);
        free(name);
        if (bhs_data) bignum_destroy(bhs_data);
        return NULL;
    }
    
    /* 恢复 group（HOOK_login 使用 get_primary_gid_by_uid，这里覆盖为原始值） */
    hook->group = (M_GID)group;
    
    /* 恢复权限 */
    hook->pm_s.ifisinit     = pm_bytes[0];
    hook->pm_s.owner_read   = pm_bytes[1];
    hook->pm_s.owner_add    = pm_bytes[2];
    hook->pm_s.owner_change = pm_bytes[3];
    hook->pm_s.group_read   = pm_bytes[4];
    hook->pm_s.group_add    = pm_bytes[5];
    hook->pm_s.group_change = pm_bytes[6];
    hook->pm_s.other_read   = pm_bytes[7];
    hook->pm_s.other_add    = pm_bytes[8];
    hook->pm_s.other_change = pm_bytes[9];
    
    *out_name = name;
    return hook;
}

/* ==================== 持久化回调 ==================== */

typedef struct {
    FILE *fp;
    int error;
    int saved_count;
} save_context_t;

static int save_hook_callback(const char *key, void *value, void *user_data) {
    save_context_t *ctx = (save_context_t*)user_data;
    HOOK *hook = (HOOK*)value;
    
    if (hook_serialize(key, hook, ctx->fp) != 0) {
        ctx->error = 1;
        return 1; /* 停止遍历 */
    }
    ctx->saved_count++;
    return 0; /* 继续 */
}

/* 保存注册表到磁盘 */
int reg_save_to_disk(const char *path) {
    if (!path || !Reg.hook_map) return -1;
    
    reg_lock();
    
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        reg_unlock();
        report(merr_open_file, "Registry", "Failed to open file for saving");
        return -1;
    }
    
    /* 写入魔数和版本 */
    uint32_t magic = 0x4D485853; /* "MHXS" */
    uint32_t version = 1;
    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    
    /* 写入HOOK数量 */
    int count = (int)hash_size(Reg.hook_map);
    fwrite(&count, sizeof(int), 1, fp);
    
    /* 遍历并保存每个HOOK */
    save_context_t ctx = { fp, 0, 0 };
    hash_foreach(Reg.hook_map, save_hook_callback, &ctx);
    
    fclose(fp);
    reg_unlock();
    
    if (ctx.error) {
        report(error, "Registry", "Error occurred during save");
        return -1;
    }
    
    return 0;
}

/* 从磁盘加载注册表 */
int reg_load_from_disk(const char *path) {
    if (!path || !Reg.hook_map) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        /* 文件不存在不算错误,可能是首次启动 */
        return 0;
    }
    
    /* 读取魔数和版本 */
    uint32_t magic, version;
    if (fread(&magic, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&version, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    if (magic != 0x4D485853) {
        report(error, "Registry", "Invalid registry file format");
        fclose(fp);
        return -1;
    }
    
    if (version != 1) {
        report(error, "Registry", "Unsupported registry file version");
        fclose(fp);
        return -1;
    }
    
    /* 读取HOOK数量 */
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    if (count < 0 || count > 10000000) {
        report(error, "Registry", "Invalid hook count in registry file");
        fclose(fp);
        return -1;
    }
    
    /* 加载每个HOOK */
    reg_lock();
    int loaded = 0;
    
    for (int i = 0; i < count; i++) {
        char *name = NULL;
        HOOK *hook = hook_deserialize(fp, &name);
        if (!hook || !name) {
            if (name) free(name);
            reg_unlock();
            fclose(fp);
            report(error, "Registry", "Failed to deserialize hook");
            return -1;
        }
        
        /* 检查是否已存在同名HOOK（避免重复） */
        if (hash_contains(Reg.hook_map, name)) {
            free(name);
            /* 不能调用 HOOK_logout：它按 hook->name 去注册表注销，
             * 同名就会把已存在的那一条删掉（与 reg_register_hook 里同一种错）。
             * 这里只释放这个尚未入表的新对象自身。 */
            if (hook->obj) bignum_destroy(hook->obj);
            if (hook->name) mstr_free(hook->name);
            free(hook);
            continue;
        }
        
        /* 注册到哈希表 */
        if (hash_put(Reg.hook_map, name, hook) != 0) {
            free(name);
            HOOK_logout(hook);
            free(hook);
            reg_unlock();
            fclose(fp);
            return -1;
        }
        
        free(name);
        loaded++;
    }
    
    reg_unlock();
    fclose(fp);
    return loaded;
}
