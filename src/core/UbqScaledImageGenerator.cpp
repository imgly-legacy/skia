#include "include/core/UbqScaledImageGenerator.h"

#include "src/core/SkPixmapPriv.h"

auto UbqScaledImageGenerator::MakeFromEncoded(sk_sp<SkData> data, int maxImageSize)
        -> std::unique_ptr<SkImageGenerator> {
    auto codec = SkCodec::MakeFromData(data);
    if (nullptr == codec) {
        return nullptr;
    }

    return std::unique_ptr<SkImageGenerator>(
            new UbqScaledImageGenerator(std::move(codec), data, maxImageSize));
}

static SkImageInfo adjust_info(SkCodec* codec, int maxImageSize) {
    SkImageInfo info = codec->getInfo();
    if (kUnpremul_SkAlphaType == info.alphaType()) {
        info = info.makeAlphaType(kPremul_SkAlphaType);
    }

    if (SkEncodedOriginSwapsWidthHeight(codec->getOrigin())) {
        info = SkPixmapPriv::SwapWidthHeight(info);
    }

    // Alter the advertised dimensions to fit into maxImageSize
    if (info.width() > maxImageSize || info.height() > maxImageSize) {
        float width = info.width();
        float height = info.height();
        float maxSize = maxImageSize;
        const auto desiredScale = std::min(maxSize / width, maxSize / height);

        // Query codec for supported dimensions
        const auto supportedDimensions = codec->getScaledDimensions(desiredScale);
        if (supportedDimensions.width() <= maxImageSize &&
            supportedDimensions.height() <= maxImageSize) {
            // Use codec-supported dimensions for faster decoding
            info = info.makeDimensions(supportedDimensions);
        } else {
            // Use desired dimensions directly
            const auto scaledDimensions =
                    SkISize::Make(static_cast<int32_t>(std::lround(desiredScale * width)),
                                  static_cast<int32_t>(std::lround(desiredScale * height)));
            info = info.makeDimensions(scaledDimensions);
        }
    }

    return info;
}

UbqScaledImageGenerator::UbqScaledImageGenerator(std::unique_ptr<SkCodec> codec,
                                                 sk_sp<SkData> data,
                                                 int maxImageSize)
        : INHERITED(adjust_info(codec.get(), maxImageSize))
        , fCodec(std::move(codec))
        , fData(std::move(data))
        , fMaxImageSize(maxImageSize) {}

sk_sp<SkData> UbqScaledImageGenerator::onRefEncodedData() { return fData; }

auto UbqScaledImageGenerator::needsScaling() const -> bool {
    return fCodec->dimensions() != getInfo().dimensions();
}

static inline auto isValidDecode(SkCodec::Result& result) -> bool {
    switch (result) {
        case SkCodec::kSuccess:
        case SkCodec::kIncompleteInput:
        case SkCodec::kErrorInInput:
            return true;
        default:
            return false;
    }
}

bool UbqScaledImageGenerator::getPixels(const SkImageInfo& info,
                                        void* pixels,
                                        size_t rowBytes,
                                        const SkCodec::Options* options) {
    SkPixmap dst(info, pixels, rowBytes);

    std::function<bool(const SkPixmap&)> decode;
    if (!needsScaling()) {  // Image is returned "as is", no fallback
        decode = [this, options](const SkPixmap& pm) {
            SkCodec::Result result = fCodec->getPixels(pm, options);
            return isValidDecode(result);
        };
    } else {  // Query
        decode = [this, options, info](const SkPixmap& pm) {
            // Ask codec to return image at requested size
            SkCodec::Result getPixelsResult = fCodec->getPixels(pm, options);
            if (isValidDecode(getPixelsResult)) {
                return true;
            } else {
                // Fall back to decode at closest dimension followed by forced downscale
                float limit = fMaxImageSize;
                float width = fCodec->getInfo().width();
                float height = fCodec->getInfo().height();
                auto scale = std::min(limit / height, limit / width);

                // Ask codec for supported size, that best matches the target scale
                SkISize decodableSize = fCodec->getScaledDimensions(scale);
                auto [image, getImageResult] = fCodec->getImage(info.makeDimensions(decodableSize));

                // Downscale decoded image into dstMap
                if (isValidDecode(getImageResult)) {
                    return image->scalePixels(pm, SkSamplingOptions(SkFilterMode::kLinear));
                }

                return false;
            }
        };
    }

    // Account for EXIF
    if (decode) {
        return SkPixmapPriv::Orient(dst, fCodec->getOrigin(), decode);
    }

    return false;
}

bool UbqScaledImageGenerator::onGetPixels(const SkImageInfo& requestInfo,
                                          void* requestPixels,
                                          size_t requestRowBytes,
                                          const Options& options) {
    return this->getPixels(requestInfo, requestPixels, requestRowBytes, nullptr);
}

bool UbqScaledImageGenerator::onQueryYUVAInfo(
        const SkYUVAPixmapInfo::SupportedDataTypes& supportedDataTypes,
        SkYUVAPixmapInfo* yuvaPixmapInfo) const {
    if (!needsScaling()) {
        return fCodec->queryYUVAInfo(supportedDataTypes, yuvaPixmapInfo);
    }

    return false;
}

bool UbqScaledImageGenerator::onGetYUVAPlanes(const SkYUVAPixmaps& yuvaPixmaps) {
    if (!needsScaling()) {
        switch (fCodec->getYUVAPlanes(yuvaPixmaps)) {
            case SkCodec::kSuccess:
            case SkCodec::kIncompleteInput:
            case SkCodec::kErrorInInput:
                return true;
            default:
                return false;
        }
    }

    return false;
}
