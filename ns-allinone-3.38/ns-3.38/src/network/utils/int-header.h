#ifndef INT_HEADER_H
#define INT_HEADER_H

#include "ns3/buffer.h"
#include <stdint.h>
#include <cstdio>

namespace ns3 {
#pragma pack(push, 1)
class IntHop{
public:
	static const uint32_t timeWidth = 24;
	static const uint32_t bytesWidth = 20;
	static const uint32_t qlenWidth = 17;
	static const uint64_t lineRateValues[8];
	union{
		struct {
			uint64_t lineRate: 64-timeWidth-bytesWidth-qlenWidth,
					 time: timeWidth,
					 bytes: bytesWidth,
					 qlen: qlenWidth;
		};
		uint32_t buf[2];
	};

	static const uint32_t byteUnit = 128;
	static const uint32_t qlenUnit = 80;
	static uint32_t multi;

	uint64_t GetLineRate(){
		return lineRateValues[lineRate];
	}
	uint64_t GetBytes(){
		return (uint64_t)bytes * byteUnit * multi;
	}
	uint32_t GetQlen(){
		return (uint32_t)qlen * qlenUnit * multi;
	}
	uint64_t GetTime(){
		return time;
	}
	void Set(uint64_t _time, uint64_t _bytes, uint32_t _qlen, uint64_t _rate){
		time = _time;
		bytes = _bytes / (byteUnit * multi);
		qlen = _qlen / (qlenUnit * multi);
		switch (_rate){
			case 25000000000lu:
				lineRate=0;break;
			case 50000000000lu:
				lineRate=1;break;
			case 100000000000lu:
				lineRate=2;break;
			case 200000000000lu:
				lineRate=3;break;
			case 400000000000lu:
				lineRate=4;break;
			default:
				printf("Error: IntHeader unknown rate: %lu\n", _rate);
				break;
		}
	}
	// void Set(uint64_t _time, uint64_t _bytes, uint32_t _qlen, uint64_t _rate){
	// 	// 修复时间值溢出
	// 	uint32_t clamped_time;
	// 	if (_time > MAX_TIME) {
	// 		std::cout << "===rhy test=== time clamped\t" << "_time:" << _time 
	// 				<< " > MAX_TIME:" << MAX_TIME << '\n';
	// 		clamped_time = MAX_TIME;
	// 	} else {
	// 		clamped_time = static_cast<uint32_t>(_time);
	// 	}
		
	// 	// 安全的位域赋值
	// 	uint64_t temp_lineRate = 0;
	// 	uint32_t temp_bytes = _bytes / (byteUnit * multi);
	// 	uint32_t temp_qlen = _qlen / (qlenUnit * multi);
		
	// 	// 设置线路速率
	// 	switch (_rate){
	// 		case 25000000000lu: temp_lineRate = 0; break;
	// 		case 50000000000lu: temp_lineRate = 1; break;
	// 		case 100000000000lu: temp_lineRate = 2; break;
	// 		case 200000000000lu: temp_lineRate = 3; break;
	// 		case 400000000000lu: temp_lineRate = 4; break;
	// 		default:
	// 			printf("Error: IntHeader unknown rate: %lu\n", _rate);
	// 			temp_lineRate = 0;
	// 			break;
	// 	}
		
	// 	// 一次性设置所有位域
	// 	lineRate = temp_lineRate;
	// 	time = clamped_time;
	// 	bytes = temp_bytes;
	// 	qlen = temp_qlen;
		
	// 	std::cout << "===rhy test=== IntHop Set completed safely" << '\n';
	// }
	uint64_t GetBytesDelta(IntHop &b){
		if (bytes >= b.bytes)
			return (bytes - b.bytes) * byteUnit * multi;
		else
			return (bytes + (1<<bytesWidth) - b.bytes) * byteUnit * multi;
	}
	uint64_t GetTimeDelta(IntHop &b){
		if (time >= b.time)
			return time - b.time;
		else
			return time + (1<<timeWidth) - b.time;
	}
private:
	static constexpr uint64_t MAX_TIME = (1UL << timeWidth) - 1;
};
#pragma pack(pop)
#pragma pack(push, 1)
class IntHeader{
public:
	static const uint32_t maxHop = 5;
	enum Mode{
		NORMAL = 0,
		TS = 1,
		PINT = 2,
		NONE
	};
	static Mode mode;
	static int pint_bytes;

	// Note: the structure of IntHeader must have no internal padding, because we will directly transform the part of packet buffer to IntHeader*
	union{
		struct {
			IntHop hop[maxHop];
			uint16_t nhop;
		};
		uint64_t ts;
		union {
			uint16_t power;
			struct{
				uint8_t power_lo8, power_hi8;
			};
		}pint;
	};
	void reset(){
		nhop = 0;
		for (uint32_t i = 0; i < maxHop; i++)
			hop[i] = {0};
	}
	IntHeader();
	static uint32_t GetStaticSize();
	void PushHop(uint64_t time, uint64_t bytes, uint32_t qlen, uint64_t rate);
	void Serialize (Buffer::Iterator start) const;
	uint32_t Deserialize (Buffer::Iterator start);
	uint64_t GetTs(void);
	uint16_t GetPower(void);
	void SetPower(uint16_t);
};
#pragma pack(pop)
}

#endif /* INT_HEADER_H */
