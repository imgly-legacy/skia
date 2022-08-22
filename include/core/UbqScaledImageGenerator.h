#ifndef UbqScaledImageGenerator_DEFINED
#define UbqScaledImageGenerator_DEFINED

#include "include/codec/SkCodec.h"
#include "include/core/SkData.h"
#include "include/core/SkImageGenerator.h"

/**
 * Given an initial `maxImageSize` and an encoded image, this generator will only yield downscaled
 * versions of the contained image, that have at most an edge length of `maxImageSize`
 * It attempts to use dimensions, that can be directly decoded from the encoded image
 * representation, which may not fully fill the size defined by `maxImageSize`, but gives a
 * performance benefit If the codec doesn't support decoding into matching dimensions, falls back to
 * in-memory downscaling via `scalePixels`, but discards the full copy of the decoded image Accounts
 * for EXIF orientation
 */
class UbqScaledImageGenerator : public SkImageGenerator {
public:
    /**
     * Creates a new generator based on the given data.
     */
    static std::unique_ptr<SkImageGenerator> MakeFromEncoded(sk_sp<SkData>,
                                                             int maxImageSize = 4096);

    /**
     *  Decode into the given pixels, a block of memory of size at
     *  least (info.fHeight - 1) * rowBytes + (info.fWidth *
     *  bytesPerPixel)
     *
     *  Repeated calls to this function should give the same results,
     *  allowing the PixelRef to be immutable.
     *
     *  @param info A description of the format
     *         expected by the caller.  This can simply be identical
     *         to the info returned by getInfo().
     *
     *         This contract also allows the caller to specify
     *         different output-configs, which the implementation can
     *         decide to support or not.
     *
     *         A size that does not match getInfo() implies a request
     *         to scale. If the generator cannot perform this scale,
     *         it will return false.
     *
     *  @return true on success.
     */
    bool getPixels(const SkImageInfo& info,
                   void* pixels,
                   size_t rowBytes,
                   const SkCodec::Options* options = nullptr);

protected:
    sk_sp<SkData> onRefEncodedData() override;

    bool onGetPixels(const SkImageInfo& info,
                     void* pixels,
                     size_t rowBytes,
                     const Options& opts) override;

    bool onQueryYUVAInfo(const SkYUVAPixmapInfo::SupportedDataTypes&,
                         SkYUVAPixmapInfo*) const override;

    bool onGetYUVAPlanes(const SkYUVAPixmaps& yuvaPixmaps) override;

private:
    /*
     * Takes ownership of codec
     */
    UbqScaledImageGenerator(std::unique_ptr<SkCodec>, sk_sp<SkData>, int maxImageSize);

    auto needsScaling() const -> bool;

    std::unique_ptr<SkCodec> fCodec;
    sk_sp<SkData> fData;
    int fMaxImageSize;

    using INHERITED = SkImageGenerator;
};
#endif  // UbqScaledImageGenerator_DEFINED
