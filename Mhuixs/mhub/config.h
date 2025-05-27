#if defined(TBLH_H) 
    && defined(KALOT_H) 
    && defined(STREAM_H)
#define BASIC_FUNCTON_OK
else
#error "Mhuixs核心的必带功能未全部定义"
#endif

#ifdef LIST_H
#define LIST_OK
#endif

#ifdef BITMAP_H
#define BITMAP_OK
#endif