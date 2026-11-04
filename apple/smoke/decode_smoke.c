#include <FFmpegAudio/FFmpegAudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int list_codecs(void) {
    const AVCodec *codec;
    const AVCodecParser *parser;
    void *it = NULL;
    int decoders = 0, encoders = 0;
    printf("license: %s\n", avcodec_license());
    printf("configuration: %s\n", avcodec_configuration());
    while ((codec = av_codec_iterate(&it))) {
        int enc = av_codec_is_encoder(codec);
        printf("codec: %s %s\n", enc ? "encoder" : "decoder", codec->name);
        enc ? encoders++ : decoders++;
    }
    it = NULL;
    while ((parser = av_parser_iterate(&it)))
        for (int i = 0; i < 7 && parser->codec_ids[i]; i++)
            printf("parser: %s\n", avcodec_get_name(parser->codec_ids[i]));
    printf("decoders: %d encoders: %d\n", decoders, encoders);
    return encoders == 0 ? 0 : 1;
}

static int drain(AVCodecContext *ctx, AVFrame *frame, long long *samples, int *frames) {
    int ret;
    while ((ret = avcodec_receive_frame(ctx, frame)) >= 0) {
        *samples += frame->nb_samples;
        (*frames)++;
        av_frame_unref(frame);
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--codecs"))
        return list_codecs();
    if (argc != 3) {
        fprintf(stderr, "usage: %s --codecs | <decoder> <file>\n", argv[0]);
        return 2;
    }
    const AVCodec *codec = avcodec_find_decoder_by_name(argv[1]);
    if (!codec) {
        fprintf(stderr, "no decoder %s\n", argv[1]);
        return 1;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f) {
        perror(argv[2]);
        return 1;
    }
    AVCodecParserContext *parser = av_parser_init(codec->id);
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!parser || !ctx || !pkt || !frame || avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "could not set up %s\n", codec->name);
        return 1;
    }
    uint8_t buf[4096 + AV_INPUT_BUFFER_PADDING_SIZE];
    long long samples = 0;
    int frames = 0, packets = 0, ret = 0;
    for (;;) {
        size_t len = fread(buf, 1, 4096, f);
        memset(buf + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        const uint8_t *data = buf;
        int eof = len == 0;
        while (len > 0 || eof) {
            int used = av_parser_parse2(parser, ctx, &pkt->data, &pkt->size, data, (int)len,
                                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (used < 0) {
                fprintf(stderr, "parser error\n");
                return 1;
            }
            data += used;
            len -= used;
            if (pkt->size) {
                packets++;
                if ((ret = avcodec_send_packet(ctx, pkt)) < 0) {
                    char err[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, err, sizeof err);
                    fprintf(stderr, "send_packet: %s\n", err);
                    return 1;
                }
                if ((ret = drain(ctx, frame, &samples, &frames)) < 0)
                    return 1;
            }
            if (eof)
                break;
        }
        if (eof)
            break;
    }
    avcodec_send_packet(ctx, NULL);
    if (drain(ctx, frame, &samples, &frames) < 0)
        return 1;
    char layout[64] = "";
    av_channel_layout_describe(&ctx->ch_layout, layout, sizeof layout);
    printf("decoder=%s packets=%d frames=%d channels=%d layout=%s sample_rate=%d sample_fmt=%s "
           "samples_per_channel=%lld profile=%s\n",
           codec->name, packets, frames, ctx->ch_layout.nb_channels, layout, ctx->sample_rate,
           av_get_sample_fmt_name(ctx->sample_fmt), samples,
           avcodec_profile_name(codec->id, ctx->profile) ? avcodec_profile_name(codec->id, ctx->profile) : "-");
    fclose(f);
    av_parser_close(parser);
    avcodec_free_context(&ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    return samples > 0 ? 0 : 1;
}
