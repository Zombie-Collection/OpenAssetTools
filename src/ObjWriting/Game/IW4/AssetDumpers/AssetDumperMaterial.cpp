#include "AssetDumperMaterial.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>

#include "Utils/ClassUtils.h"
#include "Game/IW4/MaterialConstantsIW4.h"
#include "Game/IW4/TechsetConstantsIW4.h"

#define DUMP_AS_JSON 1
#define DUMP_AS_GDT 1
//#define FLAGS_DEBUG 1

using namespace IW4;
using json = nlohmann::json;

namespace IW4
{
    const char* AssetName(const char* name)
    {
        if (name && name[0] == ',')
            return &name[1];
        return name;
    }

    template <size_t S>
    json ArrayEntry(const char* (&a)[S], const size_t index)
    {
        assert(index < S);
        if (index < S)
            return a[index];

        return json{};
    }

    json BuildComplexTableJson(const complex_s* complexTable, const size_t count)
    {
        auto jArray = json::array();

        if (complexTable)
        {
            for (auto index = 0u; index < count; index++)
            {
                const auto& entry = complexTable[index];
                jArray.emplace_back(json{
                    {"real", entry.real},
                    {"imag", entry.imag}
                });
            }
        }

        return jArray;
    }

    json BuildWaterJson(water_t* water)
    {
        if (!water)
            return json{};

        return json{
            {"floatTime", water->writable.floatTime},
            {"H0", BuildComplexTableJson(water->H0, water->M * water->N)},
            {"wTerm", water->wTerm ? json{std::vector(water->wTerm, water->wTerm + (water->M * water->N))} : json::array()},
            {"M", water->M},
            {"N", water->N},
            {"Lx", water->Lx},
            {"Lz", water->Lz},
            {"windvel", water->windvel},
            {"winddir", std::vector(std::begin(water->winddir), std::end(water->winddir))},
            {"amplitude", water->amplitude},
            {"codeConstant", std::vector(std::begin(water->codeConstant), std::end(water->codeConstant))},
            {"image", water->image && water->image->name ? AssetName(water->image->name) : nullptr}
        };
    }

    json BuildSamplerStateJson(unsigned char samplerState)
    {
        static const char* samplerFilterNames[]
        {
            "none",
            "nearest",
            "linear",
            "aniso2x",
            "aniso4x"
        };
        static const char* samplerMipmapNames[]
        {
            "disabled",
            "nearest",
            "linear"
        };

        return json{
            {"filter", ArrayEntry(samplerFilterNames, (samplerState & SAMPLER_FILTER_MASK) >> SAMPLER_FILTER_SHIFT)},
            {"mipmap", ArrayEntry(samplerMipmapNames, (samplerState & SAMPLER_MIPMAP_MASK) >> SAMPLER_MIPMAP_SHIFT)},
            {"clampU", (samplerState & SAMPLER_CLAMP_U) ? true : false},
            {"clampV", (samplerState & SAMPLER_CLAMP_V) ? true : false},
            {"clampW", (samplerState & SAMPLER_CLAMP_W) ? true : false},
        };
    }

    json BuildTextureTableJson(MaterialTextureDef* textureTable, const size_t count)
    {
        static const char* semanticNames[]
        {
            "2d",
            "function",
            "colorMap",
            "detailMap",
            "unused2",
            "normalMap",
            "unused3",
            "unused4",
            "specularMap",
            "unused5",
            "unused6",
            "waterMap"
        };

        auto jArray = json::array();

        if (textureTable)
        {
            for (auto index = 0u; index < count; index++)
            {
                const auto& entry = textureTable[index];

                json jEntry = {
                    {"samplerState", BuildSamplerStateJson(entry.samplerState)},
                    {"semantic", ArrayEntry(semanticNames, entry.semantic)}
                };

                const auto knownMaterialSourceName = knownMaterialSourceNames.find(entry.nameHash);
                if (knownMaterialSourceName != knownMaterialSourceNames.end())
                {
                    jEntry["name"] = knownMaterialSourceName->second;
                }
                else
                {
                    jEntry.merge_patch({
                        {"nameHash", entry.nameHash},
                        {"nameStart", entry.nameStart},
                        {"nameEnd", entry.nameEnd},
                    });
                }

                if (entry.semantic == TS_WATER_MAP)
                {
                    jEntry["water"] = BuildWaterJson(entry.u.water);
                }
                else
                {
                    jEntry["image"] = entry.u.image && entry.u.image->name ? AssetName(entry.u.image->name) : nullptr;
                }

                jArray.emplace_back(std::move(jEntry));
            }
        }

        return jArray;
    }

    json BuildConstantTableJson(const MaterialConstantDef* constantTable, const size_t count)
    {
        auto jArray = json::array();

        if (constantTable)
        {
            for (auto index = 0u; index < count; index++)
            {
                const auto& entry = constantTable[index];
                json jEntry = {
                    {"literal", std::vector(std::begin(entry.literal), std::end(entry.literal))}
                };

                const auto nameLen = strnlen(entry.name, std::extent_v<decltype(MaterialConstantDef::name)>);
                if (nameLen == std::extent_v<decltype(MaterialConstantDef::name)>)
                {
                    std::string fullLengthName(entry.name, std::extent_v<decltype(MaterialConstantDef::name)>);
                    const auto fullLengthHash = Common::R_HashString(fullLengthName.c_str(), 0);

                    if (fullLengthHash == entry.nameHash)
                    {
                        jEntry["name"] = fullLengthName;
                    }
                    else
                    {
                        const auto knownMaterialSourceName = knownMaterialSourceNames.find(entry.nameHash);
                        if (knownMaterialSourceName != knownMaterialSourceNames.end())
                        {
                            jEntry["name"] = knownMaterialSourceName->second;
                        }
                        else
                        {
                            jEntry.merge_patch({
                                {"nameHash", entry.nameHash},
                                {"namePart", fullLengthName}
                            });
                        }
                    }
                }
                else
                {
                    jEntry["name"] = std::string(entry.name, nameLen);
                }

                jArray.emplace_back(std::move(jEntry));
            }
        }

        return jArray;
    }

    json BuildStateBitsTableJson(const GfxStateBits* stateBitsTable, const size_t count)
    {
        static const char* blendNames[]
        {
            "disabled",
            "zero",
            "one",
            "srcColor",
            "invSrcColor",
            "srcAlpha",
            "invSrcAlpha",
            "destAlpha",
            "invDestAlpha",
            "destColor",
            "invDestColor",
        };
        static const char* blendOpNames[]
        {
            "disabled",
            "add",
            "subtract",
            "revSubtract",
            "min",
            "max"
        };
        static const char* depthTestNames[]
        {
            "always",
            "less",
            "equal",
            "lessEqual",
        };
        static const char* polygonOffsetNames[]
        {
            "0",
            "1",
            "2",
            "shadowMap",
        };
        static const char* stencilOpNames[]
        {
            "keep",
            "zero",
            "replace",
            "incrSat",
            "decrSat",
            "invert",
            "incr",
            "decr"
        };

        auto jArray = json::array();

        if (stateBitsTable)
        {
            for (auto index = 0u; index < count; index++)
            {
                const auto& entry = stateBitsTable[index];

                const auto srcBlendRgb = (entry.loadBits[0] & GFXS0_SRCBLEND_RGB_MASK) >> GFXS0_SRCBLEND_RGB_SHIFT;
                const auto dstBlendRgb = (entry.loadBits[0] & GFXS0_DSTBLEND_RGB_MASK) >> GFXS0_DSTBLEND_RGB_SHIFT;
                const auto blendOpRgb = (entry.loadBits[0] & GFXS0_BLENDOP_RGB_MASK) >> GFXS0_BLENDOP_RGB_SHIFT;
                const auto srcBlendAlpha = (entry.loadBits[0] & GFXS0_SRCBLEND_ALPHA_MASK) >> GFXS0_SRCBLEND_ALPHA_SHIFT;
                const auto dstBlendAlpha = (entry.loadBits[0] & GFXS0_DSTBLEND_ALPHA_MASK) >> GFXS0_DSTBLEND_ALPHA_SHIFT;
                const auto blendOpAlpha = (entry.loadBits[0] & GFXS0_BLENDOP_ALPHA_MASK) >> GFXS0_BLENDOP_ALPHA_SHIFT;
                const auto depthTest = (entry.loadBits[1] & GFXS1_DEPTHTEST_MASK) >> GFXS1_DEPTHTEST_SHIFT;
                const auto polygonOffset = (entry.loadBits[1] & GFXS1_POLYGON_OFFSET_MASK) >> GFXS1_POLYGON_OFFSET_SHIFT;

                const auto* alphaTest = "disable";
                if (entry.loadBits[0] & GFXS0_ATEST_GT_0)
                    alphaTest = "gt0";
                else if (entry.loadBits[0] & GFXS0_ATEST_LT_128)
                    alphaTest = "lt128";
                else if (entry.loadBits[0] & GFXS0_ATEST_GE_128)
                    alphaTest = "ge128";
                else
                    assert(entry.loadBits[0] & GFXS0_ATEST_DISABLE);

                const auto* cullFace = "none";
                if ((entry.loadBits[0] & GFXS0_CULL_MASK) == GFXS0_CULL_BACK)
                    cullFace = "back";
                else if ((entry.loadBits[0] & GFXS0_CULL_MASK) == GFXS0_CULL_FRONT)
                    cullFace = "front";
                else
                    assert((entry.loadBits[0] & GFXS0_CULL_MASK) == GFXS0_CULL_NONE);

                jArray.emplace_back(json{
                    {"srcBlendRgb", ArrayEntry(blendNames, srcBlendRgb)},
                    {"dstBlendRgb", ArrayEntry(blendNames, dstBlendRgb)},
                    {"blendOpRgb", ArrayEntry(blendOpNames, blendOpRgb)},
                    {"alphaTest", alphaTest},
                    {"cullFace", cullFace},
                    {"srcBlendAlpha", ArrayEntry(blendNames, srcBlendAlpha)},
                    {"dstBlendAlpha", ArrayEntry(blendNames, dstBlendAlpha)},
                    {"blendOpAlpha", ArrayEntry(blendOpNames, blendOpAlpha)},
                    {"colorWriteRgb", (entry.loadBits[0] & GFXS0_COLORWRITE_RGB) ? true : false},
                    {"colorWriteAlpha", (entry.loadBits[0] & GFXS0_COLORWRITE_ALPHA) ? true : false},
                    {"gammaWrite", (entry.loadBits[0] & GFXS0_GAMMAWRITE) ? true : false},
                    {"polymodeLine", (entry.loadBits[0] & GFXS0_POLYMODE_LINE) ? true : false},

                    {"depthWrite", (entry.loadBits[1] & GFXS1_DEPTHWRITE) ? true : false},
                    {"depthTest", (entry.loadBits[1] & GFXS1_DEPTHTEST_DISABLE) ? json("disable") : ArrayEntry(depthTestNames, depthTest)},
                    {"polygonOffset", ArrayEntry(polygonOffsetNames, polygonOffset)},
                    {"stencilFrontEnabled", (entry.loadBits[1] & GFXS1_STENCIL_FRONT_ENABLE) ? true : false},
                    {"stencilBackEnabled", (entry.loadBits[1] & GFXS1_STENCIL_BACK_ENABLE) ? true : false},
                    {"stencilFrontPass", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_FRONT_PASS_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilFrontFail", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_FRONT_FAIL_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilFrontZFail", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_FRONT_ZFAIL_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilFrontFunc", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_FRONT_FUNC_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilBackPass", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_BACK_PASS_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilBackFail", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_BACK_FAIL_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilBackZFail", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_BACK_ZFAIL_SHIFT) & GFXS_STENCILOP_MASK)},
                    {"stencilBackFunc", ArrayEntry(stencilOpNames, (entry.loadBits[1] >> GFXS1_STENCIL_BACK_FUNC_SHIFT) & GFXS_STENCILOP_MASK)},
                });
            }
        }

        return jArray;
    }

    json BuildCharFlagsJson(const std::string& prefix, const unsigned char gameFlags)
    {
        std::vector<std::string> values;

        for (auto i = 0u; i < (sizeof(gameFlags) * 8u); i++)
        {
            if (gameFlags & (1 << i))
            {
                std::ostringstream ss;
                ss << prefix << " 0x" << std::hex << (1 << i);
                values.emplace_back(ss.str());
            }
        }

        return json(values);
    }

    std::string CreateSurfaceTypeString(const unsigned surfaceTypeBits)
    {
        if (!surfaceTypeBits)
            return "<none>";

        static constexpr auto NON_SURFACE_TYPE_BITS = ~(std::numeric_limits<unsigned>::max() >> ((sizeof(unsigned) * 8) - (static_cast<unsigned>(SURF_TYPE_NUM) - 1)));
        assert((surfaceTypeBits & NON_SURFACE_TYPE_BITS) == 0);

        std::ostringstream ss;
        auto firstSurfaceType = true;
        for (auto surfaceTypeIndex = static_cast<unsigned>(SURF_TYPE_BARK); surfaceTypeIndex < SURF_TYPE_NUM; surfaceTypeIndex++)
        {
            if ((surfaceTypeBits & (1 << (surfaceTypeIndex - 1))) == 0)
                continue;

            if (firstSurfaceType)
                firstSurfaceType = false;
            else
                ss << ",";
            ss << surfaceTypeNames[surfaceTypeIndex];
        }

        if (firstSurfaceType)
            return "<none>";

        return ss.str();
    }

    void DumpMaterialAsJson(Material* material, std::ostream& stream)
    {
        static const char* cameraRegionNames[]
        {
            "litOpaque",
            "litTrans",
            "emissive",
            "depthHack",
            "none"
        };

        const json j = {
            {
                "info", {
#if defined(FLAGS_DEBUG) && FLAGS_DEBUG == 1
                    {"gameFlags", BuildCharFlagsJson("gameFlag", material->info.gameFlags)}, // TODO: Find out what gameflags mean
#else
                    {"gameFlags", material->info.gameFlags}, // TODO: Find out what gameflags mean
#endif
                    {"sortKey", material->info.sortKey},
                    {"textureAtlasRowCount", material->info.textureAtlasRowCount},
                    {"textureAtlasColumnCount", material->info.textureAtlasColumnCount},
                    {
                        "drawSurf", {
                            {"objectId", static_cast<unsigned>(material->info.drawSurf.fields.objectId)},
                            {"reflectionProbeIndex", static_cast<unsigned>(material->info.drawSurf.fields.reflectionProbeIndex)},
                            {"hasGfxEntIndex", static_cast<unsigned>(material->info.drawSurf.fields.hasGfxEntIndex)},
                            {"customIndex", static_cast<unsigned>(material->info.drawSurf.fields.customIndex)},
                            {"materialSortedIndex", static_cast<unsigned>(material->info.drawSurf.fields.materialSortedIndex)},
                            {"prepass", static_cast<unsigned>(material->info.drawSurf.fields.prepass)},
                            {"useHeroLighting", static_cast<unsigned>(material->info.drawSurf.fields.useHeroLighting)},
                            {"sceneLightIndex", static_cast<unsigned>(material->info.drawSurf.fields.sceneLightIndex)},
                            {"surfType", static_cast<unsigned>(material->info.drawSurf.fields.surfType)},
                            {"primarySortKey", static_cast<unsigned>(material->info.drawSurf.fields.primarySortKey)}
                        }
                    },
                    {"surfaceTypeBits", CreateSurfaceTypeString(material->info.surfaceTypeBits)},
                    {"hashIndex", material->info.hashIndex}
                }
            },
            {"stateBitsEntry", std::vector(std::begin(material->stateBitsEntry), std::end(material->stateBitsEntry))},
#if defined(FLAGS_DEBUG) && FLAGS_DEBUG == 1
            {"stateFlags", BuildCharFlagsJson("stateFlag", material->stateFlags)},
#else
            {"stateFlags", material->stateFlags},
#endif
            {"cameraRegion", ArrayEntry(cameraRegionNames, material->cameraRegion)},
            {"techniqueSet", material->techniqueSet && material->techniqueSet->name ? AssetName(material->techniqueSet->name) : nullptr},
            {"textureTable", BuildTextureTableJson(material->textureTable, material->textureCount)},
            {"constantTable", BuildConstantTableJson(material->constantTable, material->constantCount)},
            {"stateBitsTable", BuildStateBitsTableJson(material->stateBitsTable, material->stateBitsCount)}
        };

        stream << std::setw(4) << j;
    }

    enum class GdtMaterialType
    {
        MATERIAL_TYPE_UNKNOWN,
        MATERIAL_TYPE_2D,
        MATERIAL_TYPE_CUSTOM,
        MATERIAL_TYPE_DISTORTION,
        MATERIAL_TYPE_EFFECT,
        MATERIAL_TYPE_IMPACT_MARK,
        MATERIAL_TYPE_MODEL_AMBIENT,
        MATERIAL_TYPE_MODEL_PHONG,
        MATERIAL_TYPE_MODEL_UNLIT,
        MATERIAL_TYPE_OBJECTIVE,
        MATERIAL_TYPE_PARTICLE_CLOUD,
        MATERIAL_TYPE_SKY,
        MATERIAL_TYPE_TOOLS,
        MATERIAL_TYPE_UNLIT,
        MATERIAL_TYPE_WATER,
        MATERIAL_TYPE_WORLD_PHONG,
        MATERIAL_TYPE_WORLD_UNLIT,

        MATERIAL_TYPE_COUNT
    };

    enum class GdtCustomMaterialTypes
    {
        CUSTOM_MATERIAL_TYPE_NONE,
        // Uses custom techset with generic options
        CUSTOM_MATERIAL_TYPE_CUSTOM,
        CUSTOM_MATERIAL_TYPE_PHONG_FLAG,
        CUSTOM_MATERIAL_TYPE_GRAIN_OVERLAY,
        CUSTOM_MATERIAL_TYPE_EFFECT_EYE_OFFSET,
        CUSTOM_MATERIAL_TYPE_REFLEX_SIGHT,
        CUSTOM_MATERIAL_TYPE_SHADOW_CLEAR,
        CUSTOM_MATERIAL_TYPE_SHADOW_OVERLAY,

        // Not part of IW3
        CUSTOM_MATERIAL_TYPE_SPLATTER,

        CUSTOM_MATERIAL_TYPE_COUNT
    };

    const char* GdtMaterialTypeNames[]
    {
        "<unknown>",
        "2d",
        "custom",
        "distortion",
        "effect",
        "impact mark",
        "model ambient",
        "model phong",
        "model unlit",
        "objective",
        "particle cloud",
        "sky",
        "tools",
        "unlit",
        "water",
        "world phong",
        "world unlit"
    };
    static_assert(std::extent_v<decltype(GdtMaterialTypeNames)> == static_cast<size_t>(GdtMaterialType::MATERIAL_TYPE_COUNT));

    const char* GdtCustomMaterialTypeNames[]
    {
        "",
        "mtl_custom",
        "mtl_phong_flag",
        "mtl_grain_overlay",
        "mtl_effect_eyeoffset",
        "mtl_reflexsight",
        "mtl_shadowclear",
        "mtl_shadowoverlay",
        "mtl_splatter"
    };
    static_assert(std::extent_v<decltype(GdtCustomMaterialTypeNames)> == static_cast<size_t>(GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_COUNT));

    class TechsetInfo
    {
    public:
        std::string m_techset_name;
        std::string m_techset_base_name;
        std::string m_techset_prefix;
        GdtMaterialType m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_UNKNOWN;
        GdtCustomMaterialTypes m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_NONE;
        std::string m_gdt_custom_string;
        MaterialType m_engine_material_type = MTL_TYPE_DEFAULT;
        bool m_no_cast_shadow = false;
        bool m_no_receive_dynamic_shadow = false;
        bool m_no_fog = false;
        bool m_tex_scroll = false;
        bool m_uv_anim = false;
        bool m_has_color_map = false;
        bool m_has_detail_map = false;
        bool m_has_normal_map = false;
        bool m_has_detail_normal_map = false;
        bool m_has_specular_map = false;

        // TODO: Find out what p0 in techset name actually means, seems like it only does stuff for techsets using a specular texture though
        // TODO: Find out what o0 in techset name actually means, seems like it gives the colormap a blue/whiteish tint and is almost exclusively used on snow-related materials
        bool m_specular_p_flag = false;
        bool m_color_o_flag = false;
    };

    enum class BlendFunc_e
    {
        UNKNOWN,
        CUSTOM,
        REPLACE,
        BLEND,
        ADD,
        MULTIPLY,
        SCREEN_ADD
    };

    enum class BlendOp_e
    {
        UNKNOWN,
        ADD,
        SUBTRACT,
        REV_SUBTRACT,
        MIN,
        MAX,
        DISABLE
    };

    enum class CustomBlendFunc_e
    {
        UNKNOWN,
        ONE,
        ZERO,
        SRC_COLOR,
        INV_SRC_COLOR,
        SRC_ALPHA,
        INV_SRC_ALPHA,
        DST_ALPHA,
        INV_DST_ALPHA,
        DEST_COLOR,
        INV_DST_COLOR
    };

    enum class AlphaTest_e
    {
        UNKNOWN,
        ALWAYS,
        GE128
    };

    enum class DepthTest_e
    {
        UNKNOWN,
        LESS_EQUAL,
        LESS,
        EQUAL,
        ALWAYS,
        DISABLE
    };

    enum class StateBitsEnabledStatus_e
    {
        UNKNOWN,
        ENABLED,
        DISABLED
    };

    enum class CullFace_e
    {
        UNKNOWN,
        FRONT,
        BACK,
        NONE
    };

    enum class PolygonOffset
    {
        UNKNOWN,
        STATIC_DECAL,
        WEAPON_IMPACT
    };

    class StateBitsInfo
    {
    public:
        BlendFunc_e m_blend_func = BlendFunc_e::UNKNOWN;
        BlendOp_e m_custom_blend_op_rgb = BlendOp_e::UNKNOWN;
        BlendOp_e m_custom_blend_op_alpha = BlendOp_e::UNKNOWN;
        CustomBlendFunc_e m_custom_src_blend_func = CustomBlendFunc_e::UNKNOWN;
        CustomBlendFunc_e m_custom_dst_blend_func = CustomBlendFunc_e::UNKNOWN;
        CustomBlendFunc_e m_custom_src_blend_func_alpha = CustomBlendFunc_e::UNKNOWN;
        CustomBlendFunc_e m_custom_dst_blend_func_alpha = CustomBlendFunc_e::UNKNOWN;
        AlphaTest_e m_alpha_test = AlphaTest_e::UNKNOWN;
        DepthTest_e m_depth_test = DepthTest_e::UNKNOWN;
        StateBitsEnabledStatus_e m_depth_write = StateBitsEnabledStatus_e::UNKNOWN;
        CullFace_e m_cull_face = CullFace_e::UNKNOWN;
        PolygonOffset m_polygon_offset = PolygonOffset::UNKNOWN;
        StateBitsEnabledStatus_e m_color_write_rgb = StateBitsEnabledStatus_e::UNKNOWN;
        StateBitsEnabledStatus_e m_color_write_alpha = StateBitsEnabledStatus_e::UNKNOWN;
    };

    class MaterialGdtDumper
    {
        std::ostream& m_stream;

        TechsetInfo m_techset_info;
        StateBitsInfo m_state_bits_info;

        const Material* m_material;
        GdtEntry m_entry;

        void SetValue(const std::string& key, std::string value)
        {
            m_entry.m_properties.emplace(std::make_pair(key, std::move(value)));
        }

        void SetValue(const std::string& key, const float (&value)[4])
        {
            std::ostringstream ss;
            ss << value[0] << " " << value[1] << " " << value[2] << " " << value[3];
            m_entry.m_properties.emplace(std::make_pair(key, ss.str()));
        }

        template <typename T,
                  typename = typename std::enable_if_t<std::is_arithmetic_v<T>, T>>
        void SetValue(const std::string& key, T value)
        {
            m_entry.m_properties.emplace(std::make_pair(key, std::to_string(value)));
        }

        _NODISCARD int FindConstant(const std::string& constantName) const
        {
            const auto constantHash = Common::R_HashString(constantName.c_str(), 0u);

            if (m_material->constantTable)
            {
                for (auto i = 0; i < m_material->constantCount; i++)
                {
                    if (m_material->constantTable[i].nameHash == constantHash)
                        return i;
                }
            }

            return -1;
        }

        void SetConstantValues(const Material* material)
        {
            const auto colorTintIndex = FindConstant("colorTint");
            if (colorTintIndex >= 0)
                SetValue("colorTint", material->constantTable[colorTintIndex].literal);

            const auto envMapParmsIndex = FindConstant("envMapParms");
            if (envMapParmsIndex >= 0)
            {
                const auto& constant = material->constantTable[colorTintIndex];
                SetValue("envMapMin", constant.literal[0]);
            }
        }

        void SetCommonValues()
        {
            SetValue("textureAtlasRowCount", m_material->info.textureAtlasRowCount);
            SetValue("textureAtlasColumnCount", m_material->info.textureAtlasColumnCount);
            SetValue("surfaceType", CreateSurfaceTypeString(m_material->info.surfaceTypeBits));
        }

        _NODISCARD bool MaterialCouldPossiblyUseCustomTemplate() const
        {
            if (m_material->constantCount > 0)
                return false;

            if (m_material->textureTable)
            {
                static constexpr auto COLOR_MAP_HASH = Common::R_HashString("colorMap", 0u);
                static constexpr auto DETAIL_MAP_HASH = Common::R_HashString("detailMap", 0u);

                for (auto i = 0u; i < m_material->textureCount; i++)
                {
                    const auto nameHash = m_material->textureTable[i].nameHash;
                    if (nameHash != COLOR_MAP_HASH && nameHash != DETAIL_MAP_HASH)
                        return false;
                }
            }

            return true;
        }

        static std::vector<std::string> GetTechsetNameParts(const std::string& basename)
        {
            std::vector<std::string> result;

            auto partStartPosition = 0u;
            auto currentPosition = 0u;
            for (const auto& c : basename)
            {
                if (c == '_')
                {
                    result.emplace_back(basename, partStartPosition, currentPosition - partStartPosition);
                    partStartPosition = currentPosition + 1;
                }
                currentPosition++;
            }

            if (partStartPosition < basename.size())
                result.emplace_back(basename, partStartPosition);

            return result;
        }

        void ExamineEffectTechsetInfo()
        {
        }

        void ExamineLitTechsetInfo()
        {
            const auto nameParts = GetTechsetNameParts(m_techset_info.m_techset_base_name);
            bool inCustomName = false;
            bool customNameStart = true;
            std::ostringstream customNameStream;

            m_techset_info.m_no_receive_dynamic_shadow = true;
            for (const auto& namePart : nameParts)
            {
                if (namePart == "l")
                    continue;

                if (inCustomName)
                {
                    if (customNameStart)
                        customNameStart = false;
                    else
                        customNameStream << "_";
                    customNameStream << namePart;
                    continue;
                }

                // Anything after a custom part is part of its custom name
                if (namePart == "custom")
                {
                    inCustomName = true;
                    continue;
                }

                if (namePart == "scroll")
                    m_techset_info.m_tex_scroll = true;
                else if (namePart == "ua")
                    m_techset_info.m_uv_anim = true;
                else if (namePart == "nocast")
                    m_techset_info.m_no_cast_shadow = true;
                else if (namePart == "nofog")
                    m_techset_info.m_no_fog = true;
                else if (namePart == "sm" || namePart == "hsm")
                    m_techset_info.m_no_receive_dynamic_shadow = false;
                else if (namePart == "flag")
                {
                    m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                    m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_PHONG_FLAG;
                }
                else if (namePart.size() >= 2 && namePart[1] == '0')
                {
                    for (auto i = 0u; i < namePart.size(); i += 2)
                    {
                        switch (namePart[i])
                        {
                        case 'r':
                            m_state_bits_info.m_blend_func = BlendFunc_e::REPLACE;
                            m_state_bits_info.m_alpha_test = AlphaTest_e::ALWAYS;
                            break;
                        case 'a':
                            m_state_bits_info.m_blend_func = BlendFunc_e::ADD;
                            break;
                        case 'b':
                            m_state_bits_info.m_blend_func = BlendFunc_e::BLEND;
                            break;
                        case 't':
                            m_state_bits_info.m_blend_func = BlendFunc_e::REPLACE;
                            m_state_bits_info.m_alpha_test = AlphaTest_e::GE128;
                            break;
                        case 'c':
                            m_techset_info.m_has_color_map = true;
                            break;
                        case 'd':
                            m_techset_info.m_has_detail_map = true;
                            break;
                        case 'n':
                            m_techset_info.m_has_normal_map = true;
                            break;
                        case 'q':
                            m_techset_info.m_has_detail_normal_map = true;
                            break;
                        case 's':
                            m_techset_info.m_has_specular_map = true;
                            break;
                        case 'p':
                            m_techset_info.m_specular_p_flag = true;
                            break;
                        case 'o':
                            m_techset_info.m_color_o_flag = true;
                            break;
                        default:
                            assert(false);
                            break;
                        }
                    }
                }
                else
                    assert(false);
            }

            if (inCustomName)
            {
                m_techset_info.m_gdt_custom_string = customNameStream.str();
            }
        }

        void ExamineUnlitTechsetInfo()
        {
        }

        void ExamineTechsetInfo()
        {
            if (!m_material->techniqueSet || !m_material->techniqueSet->name)
                return;

            m_techset_info.m_techset_name = AssetName(m_material->techniqueSet->name);
            m_techset_info.m_techset_base_name = m_techset_info.m_techset_name;

            for (auto materialType = MTL_TYPE_DEFAULT + 1; materialType < MTL_TYPE_COUNT; materialType++)
            {
                const std::string_view techsetPrefix(g_materialTypeInfo[materialType].techniqueSetPrefix);
                if (m_techset_info.m_techset_name.rfind(techsetPrefix, 0) == 0)
                {
                    m_techset_info.m_techset_base_name = m_techset_info.m_techset_name.substr(techsetPrefix.size());
                    m_techset_info.m_techset_prefix = std::string(techsetPrefix);
                    break;
                }
            }

            if (m_techset_info.m_techset_base_name == "2d")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_2D;
            }
            else if (m_techset_info.m_techset_base_name == "tools")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_TOOLS;
            }
            else if (m_techset_info.m_techset_base_name == "objective")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_OBJECTIVE;
            }
            else if (m_techset_info.m_techset_base_name == "sky")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_SKY;
            }
            else if (m_techset_info.m_techset_base_name == "water")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_WATER;
            }
            else if (m_techset_info.m_techset_base_name.rfind("ambient_", 0) == 0)
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_MODEL_AMBIENT;
            }
            else if (m_techset_info.m_techset_base_name.rfind("distortion_", 0) == 0)
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_DISTORTION;
            }
            else if (m_techset_info.m_techset_base_name.rfind("particle_cloud", 0) == 0)
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_PARTICLE_CLOUD;
            }
            else if (m_techset_info.m_techset_base_name == "grain_overlay")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_GRAIN_OVERLAY;
            }
            else if (m_techset_info.m_techset_base_name == "effect_add_eyeoffset")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_EFFECT_EYE_OFFSET;
            }
            else if (m_techset_info.m_techset_base_name == "reflexsight")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_REFLEX_SIGHT;
            }
            else if (m_techset_info.m_techset_base_name == "shadowclear")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_SHADOW_CLEAR;
            }
            else if (m_techset_info.m_techset_base_name == "shadowoverlay")
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_SHADOW_OVERLAY;
            }
            else if (m_techset_info.m_techset_base_name.rfind("splatter", 0) == 0)
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_SPLATTER;
            }
            else if (m_techset_info.m_techset_base_name.rfind("effect", 0) == 0)
            {
                ExamineEffectTechsetInfo();
            }
            else if (m_techset_info.m_techset_base_name.rfind("l_", 0) == 0)
            {
                ExamineLitTechsetInfo();
            }
            else if (m_techset_info.m_techset_base_name.rfind("unlit", 0) == 0)
            {
                ExamineUnlitTechsetInfo();
            }
            else if (MaterialCouldPossiblyUseCustomTemplate())
            {
                m_techset_info.m_gdt_material_type = GdtMaterialType::MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_material_type = GdtCustomMaterialTypes::CUSTOM_MATERIAL_TYPE_CUSTOM;
                m_techset_info.m_gdt_custom_string = m_techset_info.m_techset_base_name;
            }
            else
            {
                std::cout << "Could not determine material type for material \"" << m_material->info.name << "\"\n";
            }
        }

        void SetMaterialTypeValues()
        {
            ExamineTechsetInfo();
            SetValue("materialType", GdtMaterialTypeNames[static_cast<size_t>(m_techset_info.m_gdt_material_type)]);
            SetValue("customTemplate", GdtCustomMaterialTypeNames[static_cast<size_t>(m_techset_info.m_gdt_custom_material_type)]);
            SetValue("customString", m_techset_info.m_gdt_custom_string);
        }

        void SetTextureTableValues()
        {
            if (m_material->textureTable == nullptr || m_material->textureCount <= 0)
                return;

            for (auto i = 0u; i < m_material->textureCount; i++)
            {
                const auto& entry = m_material->textureTable[i];
                const auto knownMaterialSourceName = knownMaterialSourceNames.find(entry.nameHash);
                if (knownMaterialSourceName == knownMaterialSourceNames.end())
                {
                    assert(false);
                    std::cout << "Unknown material texture source name hash: 0x" << std::hex << entry.nameHash << " (" << entry.nameStart << "..." << entry.nameEnd << ")\n";
                    continue;
                }

                const char* imageName;
                if (entry.semantic != TS_WATER_MAP)
                {
                    if (!entry.u.image || !entry.u.image->name)
                        continue;
                    imageName = AssetName(entry.u.image->name);
                }
                else
                {
                    if (!entry.u.water || !entry.u.water->image || !entry.u.water->image->name)
                        continue;
                    imageName = AssetName(entry.u.water->image->name);
                }

                SetValue(knownMaterialSourceName->second, imageName);
            }
        }

    public:
        explicit MaterialGdtDumper(std::ostream& stream, const Material* material)
            : m_stream(stream),
              m_material(material)
        {
            m_entry.m_gdf_name = "material.gdf";
            m_entry.m_name = m_material->info.name;
        }

        void CreateGdtEntry()
        {
            SetCommonValues();
            SetMaterialTypeValues();
            SetTextureTableValues();
        }

        void Dump()
        {
            Gdt gdt(GdtVersion("IW4", 1));
            gdt.m_entries.emplace_back(std::make_unique<GdtEntry>(std::move(m_entry)));

            GdtOutputStream::WriteGdt(gdt, m_stream);
        }
    };
}

bool AssetDumperMaterial::ShouldDump(XAssetInfo<Material>* asset)
{
    return true;
}

void AssetDumperMaterial::DumpAsset(AssetDumpingContext& context, XAssetInfo<Material>* asset)
{
    auto* material = asset->Asset();

#if defined(DUMP_AS_JSON) && DUMP_AS_JSON == 1
    {
        std::ostringstream ss;
        ss << "materials/" << asset->m_name << ".json";
        const auto assetFile = context.OpenAssetFile(ss.str());
        if (!assetFile)
            return;
        auto& stream = *assetFile;
        DumpMaterialAsJson(material, stream);
    }
#endif

#if defined(DUMP_AS_GDT) && DUMP_AS_GDT == 1
    {
        std::ostringstream ss;
        ss << "materials/" << asset->m_name << ".gdt";
        const auto assetFile = context.OpenAssetFile(ss.str());
        if (!assetFile)
            return;
        auto& stream = *assetFile;
        MaterialGdtDumper dumper(stream, material);
        dumper.CreateGdtEntry();
        dumper.Dump();
    }
#endif
}
