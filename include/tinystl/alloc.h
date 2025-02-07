#ifndef _ALLOC_H_
#define _ALLOC_H_

#include "stdlib.h"

namespace TinySTL{

	/*
	**żŐŒäĆäÖĂÆśŁŹÒÔŚÖœÚÊęÎȘ”„Î»·ÖĆä
	**ÄÚČżÊčÓĂ
	*/
	class alloc{
	private:
		enum EAlign{ ALIGN = 8};//ĐĄĐÍÇűżé”ÄÉÏ”ś±ßœç
		enum EMaxBytes{ MAXBYTES = 128};//ĐĄĐÍÇűżé”ÄÉÏÏȚŁŹłŹčę”ÄÇűżéÓÉmalloc·ÖĆä
		enum ENFreeLists{ NFREELISTS = (EMaxBytes::MAXBYTES / EAlign::ALIGN)};//free-lists”ÄžöÊę
		enum ENObjs{ NOBJS = 20};//ĂżŽÎÔöŒÓ”ÄœÚ”ăÊę
	private:
		//free-lists”ÄœÚ”ăččÔì
		union obj{
			union obj *next;
			char client[1];
		};

		static obj *free_list[ENFreeLists::NFREELISTS];
	private:
		static char *start_free;//ÄÚŽæłŰÆđÊŒÎ»ÖĂ
		static char *end_free;//ÄÚŽæłŰœáÊűÎ»ÖĂ
		static size_t heap_size;//
	private:
		//œ«bytesÉÏ”śÖÁ8”Ä±¶Êę
		static size_t ROUND_UP(size_t bytes){
			return ((bytes + EAlign::ALIGN - 1) & ~(EAlign::ALIGN - 1));
		}
		//žùŸĘÇűżéŽóĐĄŁŹŸö¶šÊčÓĂ”ÚnșĆfree-listŁŹnŽÓ0żȘÊŒŒÆËă
		static size_t FREELIST_INDEX(size_t bytes){
			return (((bytes)+EAlign::ALIGN - 1) / EAlign::ALIGN - 1);
		}
		//·”»ŰÒ»žöŽóĐĄÎȘn”Ä¶ÔÏóŁŹČążÉÄÜŒÓÈëŽóĐĄÎȘn”ÄÆäËûÇűżé”œfree-list
		static void *refill(size_t n);
		//ĆäÖĂÒ»ŽóżéżŐŒäŁŹżÉÈĘÄÉnobjsžöŽóĐĄÎȘsize”ÄÇűżé
		//ÈçčûĆäÖĂnobjsžöÇűżéÓĐËùČ»±ăŁŹnobjsżÉÄÜ»áœ””Í
		static char *chunk_alloc(size_t size, size_t& nobjs);

	public:
		static void *allocate(size_t bytes);
		static void deallocate(void *ptr, size_t bytes);
		static void *reallocate(void *ptr, size_t old_sz, size_t new_sz);
	};
}

#endif