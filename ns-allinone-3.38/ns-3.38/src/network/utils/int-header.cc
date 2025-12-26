#include "int-header.h"
#include <iostream>

namespace ns3 {

const uint64_t IntHop::lineRateValues[8] = {25000000000lu,50000000000lu,100000000000lu,200000000000lu,400000000000lu,0,0,0};
uint32_t IntHop::multi = 1;

IntHeader::Mode IntHeader::mode = NONE;
int IntHeader::pint_bytes = 2;

IntHeader::IntHeader() : nhop(0) {
	for (uint32_t i = 0; i < maxHop; i++)
		hop[i] = {0};
	// std::cout << "===rhy test=== IntHeader::IntHeader()\t"<<"nhop"<<"\t" <<nhop<<'\n';
}

uint32_t IntHeader::GetStaticSize(){
	if (mode == NORMAL){
		return sizeof(hop) + sizeof(nhop);
	}else if (mode == TS){
		return sizeof(ts);
	}else if (mode == PINT){
		return sizeof(pint);
	}else {
		return 0;
	}
}
uint32_t write_idx = 0;
void IntHeader::PushHop(uint64_t time, uint64_t bytes, uint32_t qlen, uint64_t rate){
	// only do this in INT mode
	IntHeader _ih;
	// std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"_ih.nhop"<<"\t" <<_ih.nhop<<'\n';
	// std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"&_ih.nhop"<<"\t" <<&_ih.nhop<<'\n';
	// std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"&nhop"<<"\t" <<&nhop<<'\n';
	// std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"&hop[0]"<<"\t" <<&hop[0]<<'\n';
	// std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"sizeof(hop[0])"<<"\t" <<sizeof(hop[0])<<'\n';
	if (mode == NORMAL){
		//nhop = 2;
		//std::cout << "===rhy test=== INT hop count exceeds maxHop limit\t"<<"nhop:"<<nhop<<"\tmaxHop:"<<maxHop<<'\n';
		// if (nhop >= maxHop) {
		//     std::cout << "===rhy test=== INT hop count exceeds maxHop limit\t"<<"nhop:"<<nhop<<"\tmaxHop:"<<maxHop<<'\n';
		//     nhop = 0;
		// }
		uint32_t idx = nhop % maxHop;
		hop[idx].Set(time, bytes, qlen, rate);
		nhop++;
		//std::cout << "===rhy test=== IntHeader::PushHop()\t"<<"nhop"<<"\t" <<nhop<<'\n';
		//std::cout << "===rhy test=== 2222\t"<<'\n';
		// if (maxHop == 0 || write_idx >= maxHop) {
        //     std::cout << "Buffer full! Overwriting oldest data\n";
        //     write_idx = 0;  // 可选：重置指针从头覆盖
        // }
		// // 检查hop数组边界
        // if (write_idx >= sizeof(hop)/sizeof(hop[0])) {
        //     std::cout << "CRITICAL: write_idx exceeds hop array bounds!" << '\n';
        //     write_idx = 0; 
        // }
		// std::cout << "Writing to hop[" << idx << "]" << '\n';
		// 在 Set 调用前后添加内存检查
        uint8_t* before = (uint8_t*)this;
        uint8_t* hop_ptr = (uint8_t*)&hop[idx];
        
        // std::cout << "IntHeader start: " << (void*)before << '\n';
        // std::cout << "Current hop ptr: " << (void*)hop_ptr << '\n';
        // std::cout << "Offset in IntHeader: " << (hop_ptr - before) << '\n';
		// hop[write_idx].Set(time, bytes, qlen, rate);
        // write_idx = (write_idx + 1) % maxHop;  // 环形递增
	}
}

void IntHeader::Serialize (Buffer::Iterator start) const{
	Buffer::Iterator i = start;
	if (mode == NORMAL){
		for (uint32_t j = 0; j < maxHop; j++){
			i.WriteU32(hop[j].buf[0]);
			i.WriteU32(hop[j].buf[1]);
		}
		//std::cout << "===rhy test=== IntHeader::Serialize\t"<<"nhop"<<"\t" <<nhop<<'\n';
		i.WriteU16(nhop);
	}else if (mode == TS){
		i.WriteU64(ts);
	}else if (mode == PINT){
		if (pint_bytes == 1)
			i.WriteU8(pint.power_lo8);
		else if (pint_bytes == 2)
			i.WriteU16(pint.power);
	}
}

uint32_t IntHeader::Deserialize (Buffer::Iterator start){
	Buffer::Iterator i = start;
	if (mode == NORMAL){
		for (uint32_t j = 0; j < maxHop; j++){
			hop[j].buf[0] = i.ReadU32();
			hop[j].buf[1] = i.ReadU32();
		}
		nhop = i.ReadU16();
		// std::cout << "===rhy test=== IntHeader::Deserialize\t"<<"nhop"<<"\t" <<nhop<<'\n';
	}else if (mode == TS){
		ts = i.ReadU64();
	}else if (mode == PINT){
		if (pint_bytes == 1)
			pint.power_lo8 = i.ReadU8();
		else if (pint_bytes == 2)
			pint.power = i.ReadU16();
	}
	return GetStaticSize();
}

uint64_t IntHeader::GetTs(void){
	if (mode == TS)
		return ts;
	return 0;
}

uint16_t IntHeader::GetPower(void){
	if (mode == PINT)
		return pint_bytes == 1 ? pint.power_lo8 : pint.power;
	return 0;
}
void IntHeader::SetPower(uint16_t power){
	if (mode == PINT){
		if (pint_bytes == 1)
			pint.power_lo8 = power;
		else
			pint.power = power;
	}
}

}
