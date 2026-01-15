#include <string_type.h>
#include "esp_log.h"

String128 String128_create(char* data, size_t length) {
    String128 str;
    str.length = 0;
    
    if (length > 128) {
        ESP_LOGE("StringType", "Source length too large: Returning empty pointer");
        return str;
    }

    for (int i = 0; i < length; ++i) {
        str.data[i] = data[i]; 
        str.length++;
    }

    return str;
}

void String128_copy(String128* dest, char* src, size_t src_length){
    dest->length = 0;
    if (src_length > 128) {
        ESP_LOGE("StringType", "Source length too large");
        return;
    }
    
    for (int i = 0; i < src_length; ++i) {
        dest->data[i] = src[i];
    }
}
