#include "hints.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "memory/Memory.hpp"

namespace Engine::Map::Hints {

    /////////////////////////////////////////////////////
    // Heuristic evals for different types. (Scores are normalized with softmax for classification.)

    static float _floatScore(float value) {
        if (value == 0.0f) return 0.0f;
        if (std::isnan(value)) return 0.0f;
        int exponent;
        std::frexp(value, &exponent);
        if (exponent < -10 || exponent > 10) return 0.0f;
        return 1.0f;
    }

    float blockPointerScore(
        HintContext& ctx,
        void* address
    ) {
        BlockPointer* blockPointer = reinterpret_cast<BlockPointer*>(address);
        bool isEmpty = blockPointer->count == 0;
        if (isEmpty) return 0.0f;
        void* blockBase = ctx.structure.context->mapFile->fromRelative(blockPointer->data);
        // Range check first: it's cheap and rejects nearly everything, whereas
        // isAllocated costs a syscall.
        bool isWithinTag = blockBase >= ctx.tagStart && blockBase < ctx.tagEnd;
        if (!isWithinTag) return 0.0f;
        if (!Memory::isAllocated(blockBase)) return 0.0f;
        // Counts over 0xFFFF begin to count as negative evidence for being a valid block pointer.
        float saturation = static_cast<float>(blockPointer->count) / 0xFFFF;
        return - logf(saturation);
    }

    float floatScore(
        HintContext& ctx,
        void* address
    ) {
        if (*reinterpret_cast<uint32_t*>(address) == 0x00000000) return 0.0f;
        float* value = reinterpret_cast<float*>(address);
        return _floatScore(*value);
    }

    float tagStringScore(
        HintContext& ctx,
        void* address
    ) {
        int alphaNumericCount = 0;
        char* value = reinterpret_cast<char*>(address);
        
        std::string valueStr(value, 32);
        size_t length = std::strlen(value);
        if (length < 4) return 0.0f;

        if (value == nullptr) return 0.0f;
        for (size_t i = 0; value[i] != '\0'; ++i) {
            if (std::isalnum(static_cast<unsigned char>(value[i]))) {
                ++alphaNumericCount;
            }
        }

        float ratio = static_cast<float>(alphaNumericCount) / length;
        if (ratio < 0.5f) return 0.0f;

        return ratio;
    }

    float tagReferenceScore(
        HintContext& ctx,
        void* address
    ) {
        TagReference* tagRef = reinterpret_cast<TagReference*>(address);
        Tag* tag = ctx.structure.context->mapFile->getTag(tagRef->tagHandle);
        if (!tag || !Memory::isAllocated(tag)) return 0.0f;
        bool samePath = tagRef->path.offset == tag->path.offset;
        if (!samePath) return 0.0f;
        return 1.0f;
    }

    float vecNScore(
        HintContext& ctx,
        void* address,
        int dimension
    ) {
        if (dimension <= 0) return 0.0f;
        float score = 1.0f;
        float* value = reinterpret_cast<float*>(address);

        float magnitudeSquared = 0.0f;
        for (int i = 0; i < dimension; ++i) {
            score *= _floatScore(value[i]);
            magnitudeSquared += value[i] * value[i];
        }
        float isUnit = std::abs(magnitudeSquared - 1.0f) < 0.001;
        if (isUnit) score *= 10.0f; // Very likely to be a vector if this passes.
        return score;
    }

    inline constexpr float blockPointerWeight = 10.0f;
    inline constexpr float floatWeight = 1.0f;
    inline constexpr float tagStringWeight = 2.0f;
    inline constexpr float tagReferenceWeight = 10.0f;
    inline constexpr float vec3Weight = 1.5f;
    inline constexpr float vec4Weight = 2.0f;

    static void computeScore(
        HintContext& ctx,
        void* address,
        float& score,
        Type type
    ) {
        switch (type) {
            case Type::BlockPointer:
                score = blockPointerScore(ctx, address) * blockPointerWeight;
                break;
            case Type::F32:
                score = floatScore(ctx, address) * floatWeight;
                break;
            case Type::TagString:
                score = tagStringScore(ctx, address) * tagStringWeight;
                break;
            case Type::TagReference:
                score = tagReferenceScore(ctx, address) * tagReferenceWeight;
                break;
            case Type::Vec3:
                score = vecNScore(ctx, address, 3) * vec3Weight;
                break;
            case Type::Vec4:
                score = vecNScore(ctx, address, 4) * vec4Weight;
                break;
            default:
                score = 0.0f;
                break;
        }
    }

    static void softmax(float* scores, int count) {
        float maxScore = scores[0];
        for (int i = 1; i < count; ++i) {
            if (scores[i] > maxScore) {
                maxScore = scores[i];
            }
        }
        float sumExp = 0.0f;
        for (int i = 0; i < count; ++i) {
            scores[i] = std::exp(scores[i] - maxScore);
            sumExp += scores[i];
        }
        for (int i = 0; i < count; ++i) {
            scores[i] /= sumExp;
        }
    }

    /////////////////////////////////////////////////////
    // Public API

    void computeTypeScoreDistribution(
        HintContext& ctx,
        void* address,
        float* scores
    ) {
        if (!Memory::isAllocated(address)) {
            for (int i = 0; i < static_cast<int>(TypeCount); ++i) {
                scores[i] = 0.0f;
            }
            return;
        }
        
        for (int i = 0; i < static_cast<int>(TypeCount); ++i) {
            computeScore(ctx, address, scores[i], static_cast<Type>(i));
        }
        softmax(scores, static_cast<int>(TypeCount));
    }

    float* HintCache::getScores(HintContext& ctx, void* address) {
            auto it = cache.find(address);
            if (it != cache.end()) {
                return &scores[it->second];
            }
            size_t oldSize = scores.size();
            scores.resize(oldSize + TypeCount);
            float* newScores = &scores[oldSize];
            cache[address] = oldSize;
            computeTypeScoreDistribution(ctx, address, newScores);
            return newScores;
    }
    
    Type HintCache::suggestedType(HintContext& hintCtx, void* address, float notableThreshold) {
            float* scores = getScores(hintCtx, address);
            int bestIndex = 0;
            float bestScore = scores[0];
            for (int i = 1; i < static_cast<int>(TypeCount); ++i) {
                if (scores[i] > bestScore && scores[i] >= notableThreshold) {
                    bestScore = scores[i];
                    bestIndex = i;
                }
            }
            if (bestScore < notableThreshold) return Type::U32;
            return static_cast<Type>(bestIndex);
    }

    /////////////////////////////////////////////////////
    // Problem detection: values that score as a pointer-like type but aren't
    // labelled as one.

    // Fields whose interior bytes are already accounted for and shouldn't be
    // re-interpreted. Inline structures are scanned recursively instead.
    static bool isOpaqueFieldType(Type type) {
        switch (type) {
            case Type::BlockPointer:
            case Type::TagReference:
            case Type::DataPointer:
            case Type::TagString:
            case Type::Structure:
                return true;
            default:
                return false;
        }
    }

    static void findProbableTypeProblemsInStructure(
        HintContext& ctx,
        StructureRef& structure,
        std::vector<Analysis::Problem>& out,
        int depth = 0,
        const std::atomic<bool>* cancel = nullptr
    ) {
        constexpr int maxInlineDepth = 8;
        if (!structure.valid() || !structure.node || structure.node->size == 0) return;

        char* base = (char*)structure.address;
        char* end = base + structure.node->size;
        if (base < (char*)ctx.tagStart) return;
        if (end > (char*)ctx.tagEnd) end = (char*)ctx.tagEnd;

        auto fields = structure.getFieldRefs();
        size_t fi = 0;
        HintContext localCtx = { ctx.tagStart, ctx.tagEnd, structure };

        for (char* p = base; p + 4 <= end; ) {
            if (cancel && cancel->load()) return;
            // Fields are sorted by offset; skip any that end at or before p.
            while (fi < fields.size() && (char*)fields[fi].address + fields[fi].size() <= p) ++fi;

            if (fi < fields.size() && (char*)fields[fi].address <= p) {
                FieldRef& field = fields[fi];
                Type type = field.node->type.type;
                if (isOpaqueFieldType(type)) {
                    if (type == Type::Structure && depth < maxInlineDepth) {
                        StructureRef inner = field.getStructure();
                        findProbableTypeProblemsInStructure(ctx, inner, out, depth + 1, cancel);
                    }
                    char* fieldEnd = (char*)field.address + field.size();
                    // Stay 4-aligned relative to the structure base.
                    size_t skip = ((fieldEnd - base) + 3) & ~(size_t)3;
                    p = base + (skip > (size_t)(p - base) ? skip : (size_t)(p - base) + 4);
                    ++fi;
                    continue;
                }
            }

            if (p + TypeSizes[(int)Type::TagReference] <= end &&
                tagReferenceScore(localCtx, p) > 0.0f) {
                char* e = p + TypeSizes[(int)Type::TagReference];
                out.push_back({Analysis::ProblemKind::ProbableTagReference, p, e});
                p = e;
                continue;
            }
            if (p + TypeSizes[(int)Type::BlockPointer] <= end &&
                blockPointerScore(localCtx, p) > 0.0f) {
                char* e = p + TypeSizes[(int)Type::BlockPointer];
                out.push_back({Analysis::ProblemKind::ProbableBlockPointer, p, e});
                p = e;
                continue;
            }
            p += 4;
        }
    }

    // Scans every typed span (i.e. every parsed structure instance) for
    // unlabelled BlockPointers / TagReferences.
    void findProbableTypeProblems(
        Context* context,
        const std::vector<Analysis::SpanInfo>& spans,
        void* tagStart,
        void* tagEnd,
        std::vector<Analysis::Problem>& out,
        const std::atomic<bool>* cancel
    ) {
        size_t firstNew = out.size();
        for (const auto& info : spans) {
            if (cancel && cancel->load()) return;
            StructureNode* type = info.span.type;
            if (!type || type->size == 0) continue;
            char* start = info.span.start;
            char* end = (std::min)(info.span.end, static_cast<char*>(tagEnd));
            if (start < static_cast<char*>(tagStart) || start >= end) continue;
            // Large blocks are collapsed into a single span, so walk elements.
            for (char* p = start; type->size <= static_cast<size_t>(end - p); p += type->size) {
                if (cancel && cancel->load()) return;
                StructureRef instance = { context, type, p };
                HintContext ctx = { tagStart, tagEnd, instance };
                findProbableTypeProblemsInStructure(ctx, instance, out, 0, cancel);
            }
        }

        // Overlapping spans can scan the same bytes twice.
        auto byStart = [](const Analysis::Problem& a, const Analysis::Problem& b) {
            if (a.start != b.start) return a.start < b.start;
            return (int)a.kind < (int)b.kind;
        };
        auto same = [](const Analysis::Problem& a, const Analysis::Problem& b) {
            return a.start == b.start && a.kind == b.kind;
        };
        std::sort(out.begin() + firstNew, out.end(), byStart);
        out.erase(std::unique(out.begin() + firstNew, out.end(), same), out.end());
    }

}
