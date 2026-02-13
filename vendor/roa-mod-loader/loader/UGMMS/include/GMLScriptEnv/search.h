
#include <string>
#include <vector>
#include <windows.h>

void* __impl_scan(const std::vector<uint8_t>& bytes_to_find);
void* __impl_scan_local(const std::vector<uint8_t>& bytes_to_find, void* begin, int count);

void* __impl_read_ptr(void* start);
void* __impl_absolute_address(void* source, void* relative_addr, uint8_t instr_size);