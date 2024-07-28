#pragma once
#include "json.hpp"
#include "serialise.hpp"
#include "socket.h"
namespace curve
{

class UDPSink : public CurveSink
{
  public:
    UDPSink()
    {
    }
    virtual void Sink(const DatumPackInfo &pck) override
    {
#define CURVE_SINK_CASE(typeenum, type, info)                                                                          \
    case typeenum: {                                                                                                   \
        const type *pdata = reinterpret_cast<const type *>(buffer + info.offset);                                      \
        j[info.name]      = *pdata;                                                                                    \
        break;                                                                                                         \
    }

        using json = nlohmann::json;
        json j;
        for (int i = 0; i < pck.infos.size(); i++)
        {
            FieldInfo      info   = pck.infos[i];
            const uint8_t *buffer = pck.GetBuffer().data();

            switch (info.type)
            {
                CURVE_SINK_CASE(CURVE_TYPE_BOOL, bool, info)
                CURVE_SINK_CASE(CURVE_TYPE_CHAR, char, info)
                CURVE_SINK_CASE(CURVE_TYPE_INT8, int8_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_UINT8, uint8_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_INT16, int16_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_UINT16, uint16_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_INT32, int32_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_UINT32, uint32_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_INT64, int64_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_UINT64, uint64_t, info)
                CURVE_SINK_CASE(CURVE_TYPE_FLOAT32, float, info)
                CURVE_SINK_CASE(CURVE_TYPE_FLOAT64, double, info)
            case CURVE_TYPE_NONE:
                break;
            case CURVE_TYPE_COUNT:
                break;
            }
        }
        // std::cout << "sink" << j.dump() << std::endl;

        int         socket_protocol = 1; // 1. UDP, 2.TCP
        int         socket_type     = 1;
        u_short     socket_port     = 9870;
        std::string destination_ip  = "127.0.0.1";

        static UDPClient client(socket_port, destination_ip);
        client.send_message(j.dump());
    }
};

} // namespace curve
