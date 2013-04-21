#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ZAMEQ2_URI "http://zamaudio.com/lv2/zameq2"

typedef enum {
	ZAMEQ2_INPUT  = 0,
	ZAMEQ2_OUTPUT = 1,
	ZAMEQ2_BOOSTDB1 = 2,
	ZAMEQ2_Q1 = 3,
	ZAMEQ2_FREQ1 = 4
} PortIndex;

typedef struct {
	float* input;
	float* output;
	float* boostdb1;
	float* q1;
	float* freq1;
} ZamEQ2;


float x1,x2,y1,y2;
float a0x,a1x,a2x,b0x,b1x,b2x,gainx;
//float a0y,a1y,a2y,b0y,b1y,b2y,gainy;
float srate;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	ZamEQ2* zameq2 = (ZamEQ2*)malloc(sizeof(ZamEQ2));
	srate = rate;
	return (LV2_Handle)zameq2;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	ZamEQ2* zameq2 = (ZamEQ2*)instance;

	switch ((PortIndex)port) {
	case ZAMEQ2_INPUT:
		zameq2->input = (float*)data;
		break;
	case ZAMEQ2_OUTPUT:
		zameq2->output = (float*)data;
		break;
	case ZAMEQ2_BOOSTDB1:
		zameq2->boostdb1 = (float*)data;
		break;
	case ZAMEQ2_Q1:
		zameq2->q1 = (float*)data;
		break;
	case ZAMEQ2_FREQ1:
		zameq2->freq1 = (float*)data;
		break;
	}
}

void 
peq(float G0, float G, float GB, float w0, float Dw,
        float *a0, float *a1, float *a2, float *b0, float *b1, float *b2, float *gn) {

        float F,G00,F00,num,den,G1,G01,G11,F01,F11,W2,Dww,C,D,B,A;
        F = fabs(G*G - GB*GB);
        G00 = fabs(G*G - G0*G0);
        F00 = fabs(GB*GB - G0*G0);
        num = G0*G0 * (w0*w0 - M_PI*M_PI)*(w0*w0 - M_PI*M_PI)
                + G*G * F00 * M_PI*M_PI * Dw*Dw / F;
        den = (w0*w0 - M_PI*M_PI)*(w0*w0 - M_PI*M_PI)
                + F00 * M_PI*M_PI * Dw*Dw / F;
        G1 = sqrt(num/den);
        G01 = fabs(G*G - G0*G1);
        G11 = fabs(G*G - G1*G1);
        F01 = fabs(GB*GB - G0*G1);
        F11 = fabs(GB*GB - G1*G1);
        W2 = sqrt(G11 / G00) * tan(w0/2.f)*tan(w0/2.f);
        Dww = (1.f + sqrt(F00 / F11) * W2) * tan(Dw/2.f);
        C = F11 * Dww*Dww - 2.f * W2 * (F01 - sqrt(F00 * F11));
        D = 2.f * W2 * (G01 - sqrt(G00 * G11));
        A = sqrt((C + D) / F);
        B = sqrt((G*G * C + GB*GB * D) / F);
        *gn = G1;
        *b0 = (G1 + G0*W2 + B) / (1.f + W2 + A);
        *b1 = -2.f*(G1 - G0*W2) / (1.f + W2 + A);
        *b2 = (G1 - B + G0*W2) / (1.f + W2 + A);
        *a0 = 1.f;
        *a1 = -2.f*(1.f - W2) / (1.f + W2 + A);
        *a2 = (1 + W2 - A) / (1.f + W2 + A);

        sanitize_denormal(*b1);
        sanitize_denormal(*b2);
        sanitize_denormal(*a0);
        sanitize_denormal(*a1);
        sanitize_denormal(*a2);
        sanitize_denormal(*gn);
        if (is_nan(*b0)) { *b0 = 1.f; }
}

// Works on little-endian machines only
inline bool 
is_nan(float& value ) {
    if (((*(uint32_t *) &value) & 0x7fffffff) > 0x7f800000) {
      return true;
    }
    return false;
}

// Force already-denormal float value to zero
inline void 
sanitize_denormal(float& value) {
    if (is_nan(value)) {
        value = 0.f;
    }
}

inline int 
sign(float x) {
        return (x >= 0.f ? 1 : -1);
}

inline float 
from_dB(float gdb) {
        return (exp(gdb/20.f*log(10.f)));
}

inline float
to_dB(float g) {
        return (20.f*log10(g));
}

static void
activate(LV2_Handle instance)
{
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	ZamEQ2* zameq2 = (ZamEQ2*)instance;

	const float* const input  = zameq2->input;
	float* const       output = zameq2->output;
	const float        boostdb1 = *(zameq2->boostdb1);
	const float        q1 = *(zameq2->q1);
	const float        freq1 = *(zameq2->freq1);

	float dcgain1 = 1.f;
	float boost1 = from_dB(boostdb1);
  	float fc1 = freq1 / srate;

	//12dB boost at 1000Hz Q=5
	//float boost1 = exp(12.f/20.f*log(10.f));
	//float q1 = 5.f;
	//float fc1 = 1000.f/srate;

	float w01 = fc1*2.f*M_PI;

	float bwgain1 = (boostdb1 == 0.f) ? 1.f : (boostdb1 < 0.f) ? boost1*from_dB(3.f) : boost1*from_dB(-3.f);
	float bw1 = fc1 / q1;

	uint32_t i = 0;

	peq(dcgain1,boost1,bwgain1,w01,bw1,&a0x,&a1x,&a2x,&b0x,&b1x,&b2x,&gainx);
	//printf("B = [%f, %f, %f]\n",b0x, b1x, b2x);
	//printf("A = [%f, %f, %f]\n",a0x, a1x, a2x);
	//printf("in(0) = %f\n\n",p(3)[0]);

	for (uint32_t pos = 0; pos < n_samples; pos++) {
		sanitize_denormal(x1);
		sanitize_denormal(x2);
		sanitize_denormal(y1);
		sanitize_denormal(y2);
		output[pos] = (input[pos] * b0x + x1 * b1x + x2 * b2x - y1 * a1x - y2 * a2x);
		sanitize_denormal(output[pos]);
		x2 = x1;
		y2 = y1;
		x1 = input[pos];
		y1 = output[pos];
	}
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	ZAMEQ2_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
