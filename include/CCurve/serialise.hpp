#pragma once

#include <array>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace curve
{

template <typename T>
inline constexpr bool is_number()
{
    return std::is_arithmetic<T>::value || std::is_enum<T>::value;
}

enum FieldType
{
    CURVE_TYPE_NONE = -1,
    CURVE_TYPE_BOOL,
    CURVE_TYPE_CHAR,
    CURVE_TYPE_INT8,
    CURVE_TYPE_UINT8,

    CURVE_TYPE_INT16,
    CURVE_TYPE_UINT16,

    CURVE_TYPE_INT32,
    CURVE_TYPE_UINT32,

    CURVE_TYPE_INT64,
    CURVE_TYPE_UINT64,

    CURVE_TYPE_FLOAT32,
    CURVE_TYPE_FLOAT64,
    CURVE_TYPE_COUNT
};

struct FieldInfo
{
    std::string name;
    FieldType   type;
    size_t      size;
    size_t      offset;
};

class DatumPackInfo
{
  public:
    std::string                name;
    std::vector<FieldInfo>     infos;
    std::vector<uint8_t>       dftBuffers;
    std::vector<uint8_t>       buffer;
    std::map<std::string, int> dataIndexs;

    const std::vector<uint8_t> &GetBuffer() const
    {
        return buffer;
    }

    template <typename T>
    DatumPackInfo &Regist(const std::string &dataName, const T &dftValue)
    {
        FieldInfo info;
        info.name      = dataName;
        FieldType type = GetFieldType<T>();
        info.type      = type;
        info.size      = GetFieldTypeSize(type);
        info.offset    = dftBuffers.size();

        for (int i = 0; i < info.size; i++)
        {
            dftBuffers.push_back(uint8_t(0));
        }

        T *pdata = reinterpret_cast<T *>(&dftBuffers[info.offset]);
        *pdata   = dftValue;

        dataIndexs[dataName] = infos.size();
        infos.push_back(info);
        return *this;
    }

    template <typename T>
    void SetData(const std::string &dataName, const T &data)
    {
        if (dataIndexs.count(dataName) == 0)
        {
            std::cout << "given data name " << dataName << " has not been registrated before" << std::endl;
            return;
        }
        FieldType        type = GetFieldType<T>();
        const FieldInfo &info = infos[dataIndexs[dataName]];

        if (type != info.type)
        {
            std::cout << "given data with type" << GetFieldTypeName(type) << " differ from "
                      << GetFieldTypeName(info.type) << std::endl;
        }

        if (buffer.size() != dftBuffers.size())
        {
            ResetBuffer();
        }
        T *pdata = reinterpret_cast<T *>(&buffer[info.offset]);
        *pdata   = data;

        infos.push_back(info);
    }

    void ResetBuffer()
    {
        buffer.assign(dftBuffers.begin(), dftBuffers.end());
    }

    template <typename T>
    static FieldType   GetFieldType();
    static std::string GetFieldTypeName(const FieldType type);
    static size_t      GetFieldTypeSize(const FieldType type);
};

class CurveSink
{
  public:
    CurveSink()          = default;
    virtual ~CurveSink() = default;

    virtual void Sink(const DatumPackInfo &pck) = 0;
};

class CurveManager
{
  public:
    static CurveManager &instance()
    {
        static CurveManager instance;
        return instance;
    }

    void Build(const std::string &pkgName, const DatumPackInfo &info)
    {
        std::lock_guard<std::mutex> lock(mtxAllCurveInfo_);
        allCurveInfo_[pkgName] = info;
        std::cout << "Regist : " << pkgName << std::endl;
        for (int i = 0; i < info.infos.size(); i++)
        {
            FieldInfo finfo = info.infos[i];
            std::cout << "\t" << std::setw(10) << finfo.name << " " << finfo.size << " " << std::setw(10)
                      << DatumPackInfo::GetFieldTypeName(finfo.type) << std::endl;
        }
    }

    void Sink(const std::string &pkgName, const DatumPackInfo &info)
    {
        std::lock_guard<std::mutex> lock(mtxAllCurveInfo_);
        if (allCurveInfo_.count(pkgName) && specSinks_.count(pkgName) && specSinks_[pkgName])
        {
            specSinks_[pkgName]->Sink(info);
        }
        if (normalSink_)
        {
            normalSink_->Sink(info);
        }
    }

    void RegistSink(const std::string &pkgName, const std::shared_ptr<CurveSink> psink)
    {
        std::lock_guard<std::mutex> lock(mtxAllCurveInfo_);
        if (allCurveInfo_.count(pkgName) == 0)
        {
            std::cout << "Regist Sink for " << pkgName << " faild" << std::endl;
            return;
        }
        specSinks_[pkgName] = psink;
    }

    void RegistNormalSink(const std::shared_ptr<CurveSink> psink)
    {
        std::lock_guard<std::mutex> lock(mtxAllCurveInfo_);
        normalSink_ = psink;
    }

  private:
    CurveManager()  = default;
    ~CurveManager() = default;

    std::mutex                 mtxAllCurveInfo_;
    std::shared_ptr<CurveSink> normalSink_{nullptr};

    std::map<std::string, DatumPackInfo>              allCurveInfo_;
    std::map<std::string, std::shared_ptr<CurveSink>> specSinks_;
};

class CurveRegistry
{
  public:
    void Regist(const std::string &pkgName, const DatumPackInfo &pck)
    {
        if (allCurveInfo_.count(pkgName))
        {
            return;
        }
        allCurveInfo_[pkgName] = pck;
        CurveManager::instance().Build(pkgName, pck);
    }

    template <typename T>
    void SetData(const std::string &pkgName, const std::string &dataName, const T &data)
    {
        if (allCurveInfo_.count(pkgName) == 0)
        {
            std::cout << "given pkg name " << pkgName << " have not been registrated before " << std::endl;
            return;
        }
        allCurveInfo_[pkgName].SetData(dataName, data);
    }

    void Sink()
    {
        for (auto &p : allCurveInfo_)
        {
            CurveManager::instance().Sink(p.first, p.second);
            p.second.ResetBuffer();
        }
    }

    std::map<std::string, DatumPackInfo> allCurveInfo_;
};

template <typename T>
FieldType DatumPackInfo::GetFieldType()
{
    if (is_number<T>() == false)
    {
        std::cout << "given type is not number " << std::endl;
        return CURVE_TYPE_NONE;
    }
    if (std::is_same<T, bool>::value)
    {
        return CURVE_TYPE_BOOL;
    }
    else if (std::is_same<T, char>::value)
    {
        return CURVE_TYPE_CHAR;
    }
    else if (std::is_same<T, int8_t>::value)
    {
        return CURVE_TYPE_INT8;
    }
    else if (std::is_same<T, uint8_t>::value)
    {
        return CURVE_TYPE_UINT8;
    }
    else if (std::is_same<T, int16_t>::value)
    {
        return CURVE_TYPE_INT16;
    }
    else if (std::is_same<T, uint16_t>::value)
    {
        return CURVE_TYPE_UINT16;
    }
    else if (std::is_same<T, int32_t>::value)
    {
        return CURVE_TYPE_INT32;
    }
    else if (std::is_same<T, uint32_t>::value)
    {
        return CURVE_TYPE_UINT32;
    }
    else if (std::is_same<T, int64_t>::value)
    {
        return CURVE_TYPE_INT64;
    }
    else if (std::is_same<T, uint64_t>::value)
    {
        return CURVE_TYPE_UINT64;
    }
    else if (std::is_same<T, float>::value)
    {
        return CURVE_TYPE_FLOAT32;
    }
    else if (std::is_same<T, double>::value)
    {
        return CURVE_TYPE_FLOAT64;
    }
    else
    {
        return CURVE_TYPE_NONE;
    }
}

inline std::string DatumPackInfo::GetFieldTypeName(FieldType type)
{
    static std::array<const std::string, FieldType::CURVE_TYPE_COUNT> kNames{
        "bool", "char", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64"};
    if (type == CURVE_TYPE_NONE || type == CURVE_TYPE_COUNT)
    {
        return "";
    }
    return kNames[type];
}

inline size_t DatumPackInfo::GetFieldTypeSize(FieldType type)
{
    static std::array<size_t, FieldType::CURVE_TYPE_COUNT> kTypeSize{
        sizeof(bool),    sizeof(char),     sizeof(int8_t),  sizeof(uint8_t),  sizeof(int16_t), sizeof(uint16_t),
        sizeof(int32_t), sizeof(uint32_t), sizeof(int64_t), sizeof(uint64_t), sizeof(float),   sizeof(double)};
    if (type == CURVE_TYPE_NONE || type == CURVE_TYPE_COUNT)
    {
        return 0;
    }
    return kTypeSize[type];
}

#define CURVE_REGISTRY_ENABLE curve::CurveRegistry __kCurveRegistry;
#define CURVE_REGISTRY(name, seq)                                                                                      \
    {                                                                                                                  \
        __kCurveRegistry.Regist(                                                                                       \
            name, std::move(curve::DatumPackInfo() CURVE_REGISTRY_impl_end(CURVE_REGISTRY_impl_decl_loop_a seq)));     \
    }

#define CURVE_REGISTRY_impl_end(...) CURVE_REGISTRY_impl_end_(__VA_ARGS__)
#define CURVE_REGISTRY_impl_end_(...) __VA_ARGS__##_end

#define CURVE_REGISTRY_impl_decl_loop(type, name, dftvalue) .Regist(name, type(dftvalue))
#define CURVE_REGISTRY_impl_decl_loop_a(...) CURVE_REGISTRY_impl_decl_loop(__VA_ARGS__) CURVE_REGISTRY_impl_decl_loop_b
#define CURVE_REGISTRY_impl_decl_loop_b(...) CURVE_REGISTRY_impl_decl_loop(__VA_ARGS__) CURVE_REGISTRY_impl_decl_loop_a
#define CURVE_REGISTRY_impl_decl_loop_a_end
#define CURVE_REGISTRY_impl_decl_loop_b_end

#define CURVE_REGISTRY_NORMAL_SINK(sink) curve::CurveManager::instance().RegistNormalSink(std::move(sink))
#define CURVE_REGISTRY_SINK(name, sink) curve::CurveManager::instance().RegistSink(name, std::move(sink))

#define CURVE_RECORD(pkgname, dataname, data) __kCurveRegistry.SetData(pkgname, dataname, data)
#define CURVE_FLUSH __kCurveRegistry.Sink();

} // namespace curve
