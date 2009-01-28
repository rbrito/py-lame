/*
 *   Copyright (c) 2001-2002 Alexander Leidinger. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 */

/* $Id$ */


#include <Python.h>
#include <lame/lame.h>
#include <stdio.h>


static void
quiet_lib_printf( const char *format, va_list ap )
{
    return;
}


/* Declarations for objects of type lame.encoder */

typedef struct {
    PyObject_HEAD
    /* XXXX Add your own stuff here */
    lame_global_flags*  gfp;
    unsigned char*      mp3_buf;
    int                 num_samples;
} Encoder;


/* BEGIN lame.encoder methods. */

static PyObject *
mp3enc_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Encoder *self;

    self = (Encoder *)type->tp_alloc(type, 0);
    if (NULL != self) {
        self->gfp = lame_init();
        if (NULL == self->gfp) {
            PyErr_SetString((PyObject *)self, "Can't initialize LAME.");
            Py_DECREF(self);
            return NULL;
        }

        /* Silence the chatty lame */
        lame_set_errorf(self->gfp, quiet_lib_printf);
        lame_set_debugf(self->gfp, quiet_lib_printf);
        lame_set_msgf(self->gfp, quiet_lib_printf);
    }

    return (PyObject *)self;
}


static void
mp3enc_dealloc(Encoder* self)
{
    if (NULL != self->gfp) {
        lame_close(self->gfp);
        self->gfp = NULL;
    }

    if (NULL != self->mp3_buf) {
        PyMem_Free(self->mp3_buf);
        self->mp3_buf = NULL;
    }

    self->ob_type->tp_free((PyObject*)self);
}


static char mp3enc_init__doc__[] =
"Initializates the internal state of LAME.\n"
"No paramteres. First you have to use the set_*() functions.\n"
"C function: lame_init_params()\n"
;

static PyObject *
mp3enc_init(Encoder *self, PyObject *args)
{
    int nsamples;

    /* Allocate an internal buffer for the MP3 output. */
    nsamples = lame_get_num_samples(self->gfp);
    self->mp3_buf = PyMem_Malloc(1.25 * nsamples + 7200);

    if (NULL == self->mp3_buf) {
        PyErr_SetString((PyObject *)self, "No MP3 buffer." );
        return NULL;
    }

    if (0 > lame_init_params( self->gfp )) {
        PyErr_SetString((PyObject *)self, "Can't initialize LAME parameters.");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_encode_interleaved__doc__[] =
"Encode interleaved audio data (2 channels, 16 bit per sample).\n"
"Parameter: audiodata\n"
"C function: lame_encode_buffer_interleaved()\n"
;
static PyObject *
mp3enc_encode_interleaved(Encoder *self, PyObject *args)
{
    int16_t* pcm;
    int      num_samples;
    int      mp3_data_size;
    int      num_channels;

    if ( !PyArg_ParseTuple( args, "s#", &pcm, &num_samples ) )
        return NULL;

    num_channels = lame_get_num_channels(self->gfp);

    if ( self->num_samples < num_samples ) {
	unsigned char *new_buf;

	new_buf = PyMem_realloc(self->mp3_buf, 1.25*num_samples + 7200);
	if ( NULL == new_buf ) {
	    PyErr_NoMemory();
	    return NULL;
	}

	self->mp3_buf = new_buf;
	self->num_samples = num_samples;
    }

    mp3_data_size = lame_encode_buffer_interleaved(
                        self->gfp,
                        pcm,
                        num_samples / (num_channels * 2), /* 16bit! */
                        self->mp3_buf,
                        self->num_samples );

    if ( 0 > mp3_data_size ) {
        switch ( mp3_data_size ) {
            case -1:
                PyErr_SetString( (PyObject *)self,
                    "mp3buf too small (this shouldn't happen, please report)" );
                return NULL;
            case -2:
                PyErr_NoMemory();
                return NULL;
            case -3:
                PyErr_SetString( (PyObject *)self,
                    "init_parameters() not called (a bug in your program)" );
                return NULL;
            case -4:
                PyErr_SetString( (PyObject *)self,
                    "psycho acoustic problems" );
                return NULL;
            case -5:
                PyErr_SetString( (PyObject *)self,
                    "ogg cleanup encoding error (this shouldn't happen, there's no ogg support)" );
                return NULL;
            case -6:
                PyErr_SetString( (PyObject *)self,
                    "ogg frame encoding error (this shouldn't happen, there's no ogg support)" );
                return NULL;
            default:
                PyErr_SetString( (PyObject *)self,
                    "unknown error, please report" );
                return NULL;
        }
    }

    return Py_BuildValue( "s#", self->mp3_buf, mp3_data_size );
}


static char mp3enc_flush_buffers__doc__[] =
"Encode remaining samples and flush the MP3 buffer.\n"
"No parameters.\n"
"C function: lame_encode_flush()\n"
;

static PyObject *
mp3enc_flush_buffers(Encoder *self, PyObject *args)
{
    int mp3_buf_fill_size;

    mp3_buf_fill_size = lame_encode_flush( self->gfp, self->mp3_buf,
                                           self->num_samples );

    if ( 0 > mp3_buf_fill_size ) {
        switch ( mp3_buf_fill_size ) {
            case -1:
                PyErr_SetString( (PyObject *)self,
                    "mp3buf too small (this shouldn't happen, please report)" );
                return NULL;
            case -2:
                PyErr_NoMemory();
                return NULL;
            case -3:
                PyErr_SetString( (PyObject *)self,
                    "init_parameters() not called (a bug in your program)" );
                return NULL;
            case -4:
                PyErr_SetString( (PyObject *)self,
                    "psycho acoustic problems" );
                return NULL;
            case -5:
                PyErr_SetString( (PyObject *)self,
                    "ogg cleanup encoding error (this shouldn't happen, there's no ogg support)" );
                return NULL;
            case -6:
                PyErr_SetString( (PyObject *)self,
                    "ogg frame encoding error (this shouldn't happen, there's no ogg support)" );
                return NULL;
            default:
                PyErr_SetString( (PyObject *)self,
                    "unknown error, please report" );
                return NULL;
        }
    }

    return Py_BuildValue( "s#", self->mp3_buf, mp3_buf_fill_size );
}



static char mp3enc_set_num_samples__doc__[] =
"Set the number of samples.\n"
"Default: 2^32-1\n"
"Parameter: int\n"
"C function: lame_set_num_samples()\n"
;

static PyObject *
mp3enc_set_num_samples(self, args)
    Encoder *self;
    PyObject *args;
{
    unsigned long num_samples;

    if ( !PyArg_ParseTuple( args, "l", &num_samples ) )
        return NULL;

    if ( 0 > lame_set_num_samples( self->gfp, num_samples ) ) {
        PyErr_SetString( (PyObject *)self, "can't set number of samples" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char mp3enc_set_out_samplerate__doc__[] =
"Set output samplerate (in Hz).\n"
"Default: 0 (let LAME choose one)\n"
"Parameter: int\n"
"C function: lame_set_out_samplerate()\n"
;

static PyObject *
mp3enc_set_out_samplerate(self, args)
    Encoder *self;
    PyObject *args;
{
    int out_samplerate;

    if ( !PyArg_ParseTuple( args, "i", &out_samplerate ) )
        return NULL;

    if ( 0 > lame_set_out_samplerate( self->gfp, out_samplerate ) ) {
        PyErr_SetString( (PyObject *)self, "can't set output samplerate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_analysis__doc__[] =
"Collect data for an MP3 frame analyzer.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_analysis\n"
;

static PyObject *
mp3enc_set_analysis(self, args)
    Encoder *self;
    PyObject *args;
{
    int analysis;

    if ( !PyArg_ParseTuple( args, "i", &analysis ) )
        return NULL;

    if ( 0 > lame_set_analysis( self->gfp, analysis ) ) {
        PyErr_SetString( (PyObject *)self, "can't set analysis mode" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_write_vbr_tag__doc__[] =
"Write a Xing VBR header frame.\n"
"Default: 1\n"
"Paramter: int\n"
"C function: lame_set_bWriteVbrTag()\n"
;

static PyObject *
mp3enc_set_write_vbr_tag(self, args)
    Encoder *self;
    PyObject *args;
{
    int write_vbr_tag;

    if ( !PyArg_ParseTuple( args, "i", &write_vbr_tag ) )
        return NULL;

    if ( 0 > lame_set_bWriteVbrTag( self->gfp, write_vbr_tag ) ) {
        PyErr_SetString( (PyObject *)self, "can't set write_vbr_tag" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_quality__doc__[] =
"Choose quality.\n"
"Range: 0 (best) - 9 (worst)\n"
"Parameter: int\n"
"C function: lame_set_quality()\n"
;

static PyObject *
mp3enc_set_quality(self, args)
    Encoder *self;
    PyObject *args;
{
    int quality;

    if ( !PyArg_ParseTuple( args, "i", &quality ) )
        return NULL;

    if ( 0 > lame_set_quality( self->gfp, quality ) ) {
        PyErr_SetString( (PyObject *)self, "can't set quality" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_mode__doc__[] =
"Set mode.\n"
"Default: let LAME choose\n"
"Parameter: lame.STEREO, lame.JOINT_STEREO, lame.DUAL, lame.MONO\n"
"C function: lame_set_mode\n"
;

static PyObject *
mp3enc_set_mode(self, args)
    Encoder *self;
    PyObject *args;
{
    int mode;

    if ( !PyArg_ParseTuple( args, "i", &mode ) )
        return NULL;

    if ( 0 > lame_set_mode( self->gfp, mode ) ) {
        PyErr_SetString( (PyObject *)self, "can't set mode" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_force_ms__doc__[] =
"Force M/S for all frames (purpose: for testing only).\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_force_ms()\n"
;

static PyObject *
mp3enc_set_force_ms(self, args)
    Encoder *self;
    PyObject *args;
{
    int force_ms;

    if ( !PyArg_ParseTuple( args, "i", &force_ms ) )
        return NULL;

    if ( 0 > lame_set_force_ms( self->gfp, force_ms ) ) {
        PyErr_SetString( (PyObject *)self, "can't force all frames to M/S" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_free_format__doc__[] =
"Use free format.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_free_format()\n"
;

static PyObject *
mp3enc_set_free_format(self, args)
    Encoder *self;
    PyObject *args;
{
    int free_format;

    if ( !PyArg_ParseTuple( args, "i", &free_format ) )
        return NULL;

    if ( 0 > lame_set_free_format( self->gfp, free_format ) ) {
        PyErr_SetString( (PyObject *)self, "can't set to free format" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_bitrate__doc__[] =
"Set bitrate.\n"
"Default: 128\n"
"Paramter: int\n"
"C function: lame_set_brate()\n"
;

static PyObject *
mp3enc_set_bitrate(self, args)
    Encoder *self;
    PyObject *args;
{
    int brate;

    if ( !PyArg_ParseTuple( args, "i", &brate ) )
        return NULL;

    if ( 0 > lame_set_brate( self->gfp, brate ) ) {
        PyErr_SetString( (PyObject *)self, "can't set bitrate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_compression_ratio__doc__[] =
"Set compression ratio.\n"
"Default: 11.0\n"
"Paramter: float\n"
"C function: lame_set_compression_ratio()\n"
;

static PyObject *
mp3enc_set_compression_ratio(self, args)
    Encoder *self;
    PyObject *args;
{
    float compression_ratio;

    if ( !PyArg_ParseTuple( args, "f", &compression_ratio ) )
        return NULL;

    if ( 0 > lame_set_compression_ratio( self->gfp, compression_ratio ) ) {
        PyErr_SetString( (PyObject *)self, "can't set compression ratio" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_preset__doc__[] =
"Set a built-in preset.  Paramter: int (bitrate) or any of the\n"
"lame.PRESET_* contstants.\n"
"C function: lame_set_preset()\n"
;

static PyObject *
mp3enc_set_preset(self, args)
    Encoder *self;
    PyObject *args;
{
    int preset;

    if ( !PyArg_ParseTuple( args, "i", &preset ) )
        return NULL;

    if ( 0 > lame_set_preset( self->gfp, preset ) ) {
        PyErr_SetString( (PyObject *)self, "can't set preset" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_asm_optimizations__doc__[] =
"Disable specific asm optimizations (if compiled in).\n"
"Paramters: lame.ASM_MMX, lame.ASM_3DNOW, lame.ASM_SSE;\n"
"           int (0 = disable)\n"
"C function: lame_set_asm_optimizations()\n"
;

static PyObject *
mp3enc_set_asm_optimizations(self, args)
    Encoder *self;
    PyObject *args;
{
    int val1, val2;

    if ( !PyArg_ParseTuple( args, "ii", &val1, &val2 ) )
        return NULL;

    if ( 0 > lame_set_asm_optimizations( self->gfp, val1, val2 ) ) {
        PyErr_SetString( (PyObject *)self, "can't set asm optimizations" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_copyright__doc__[] =
"Set copyright bit.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_copyright()\n"
;

static PyObject *
mp3enc_set_copyright(self, args)
    Encoder *self;
    PyObject *args;
{
    int copyright;

    if ( !PyArg_ParseTuple( args, "i", &copyright ) )
        return NULL;

    if ( 0 > lame_set_copyright( self->gfp, copyright ) ) {
        PyErr_SetString( (PyObject *)self, "can't set copyright bit" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_original__doc__[] =
"Set original bit.\n"
"Default: 1 (enabled)\n"
"Parameter: int\n"
"C function: lame_set_original()\n"
;

static PyObject *
mp3enc_set_original(self, args)
    Encoder *self;
    PyObject *args;
{
    int original;

    if ( !PyArg_ParseTuple( args, "i", &original ) )
        return NULL;

    if ( 0 > lame_set_original( self->gfp, original ) ) {
        PyErr_SetString( (PyObject *)self, "can't set original bit" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_error_protection__doc__[] =
"Add error protection (uses 2 bytes from each frame for CRC).\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_error_protection()\n"
;

static PyObject *
mp3enc_set_error_protection(self, args)
    Encoder *self;
    PyObject *args;
{
    int error_protection;

    if ( !PyArg_ParseTuple( args, "i", &error_protection ) )
        return NULL;

    if ( 0 > lame_set_error_protection( self->gfp, error_protection ) ) {
        PyErr_SetString( (PyObject *)self, "can't set error protection" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_extension__doc__[] =
"Set extension bit (meaningless).\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_extension()\n"
;

static PyObject *
mp3enc_set_extension(self, args)
    Encoder *self;
    PyObject *args;
{
    int extension;

    if ( !PyArg_ParseTuple( args, "i", &extension ) )
        return NULL;

    if ( 0 > lame_set_extension( self->gfp, extension ) ) {
        PyErr_SetString( (PyObject *)self, "can't set extension bit" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_strict_iso__doc__[] =
"Enforce strict ISO compliance.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_strict_ISO()\n"
;

static PyObject *
mp3enc_set_strict_iso(self, args)
    Encoder *self;
    PyObject *args;
{
    int strict_iso;

    if ( !PyArg_ParseTuple( args, "i", &strict_iso ) )
        return NULL;

    if ( 0 > lame_set_strict_ISO( self->gfp, strict_iso ) ) {
        PyErr_SetString( (PyObject *)self, "can't set strict ISO compliance" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_disable_reservoir__doc__[] =
"Disable the bit reservoir (purpose: for testing only).\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_disable_reservoir()\n"
;

static PyObject *
mp3enc_set_disable_reservoir(self, args)
    Encoder *self;
    PyObject *args;
{
    int disable_reservoir;

    if ( !PyArg_ParseTuple( args, "i", &disable_reservoir ) )
        return NULL;

    if ( 0 > lame_set_disable_reservoir( self->gfp, disable_reservoir ) ) {
        PyErr_SetString( (PyObject *)self, "can't set disable_reservoir" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_exp_quantization__doc__[] =
"Select a different 'best quantization' function (experimental!).\n"
"Default: 0\n"
"Parameter: int\n"
"C function: lame_set_experimentalX()\n"
;

static PyObject *
mp3enc_set_exp_quantization(self, args)
    Encoder *self;
    PyObject *args;
{
    int quantization;

    if ( !PyArg_ParseTuple( args, "i", &quantization ) )
        return NULL;

    if ( 0 > lame_set_experimentalX( self->gfp, quantization ) ) {
        PyErr_SetString( (PyObject *)self, "can't choose quantization function" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_exp_y__doc__[] =
"Experimental option.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_experimentalY()\n"
;

static PyObject *
mp3enc_set_exp_y(self, args)
    Encoder *self;
    PyObject *args;
{
    int y;

    if ( !PyArg_ParseTuple( args, "i", &y ) )
        return NULL;

    if ( 0 > lame_set_experimentalY( self->gfp, y ) ) {
        PyErr_SetString( (PyObject *)self, "can't set exp_y" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_exp_z__doc__[] =
"Experimantal option.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_experimentalZ()\n"
;

static PyObject *
mp3enc_set_exp_z(self, args)
    Encoder *self;
    PyObject *args;
{
    int z;

    if ( !PyArg_ParseTuple( args, "i", &z ) )
        return NULL;

    if ( 0 > lame_set_experimentalZ( self->gfp, z ) ) {
        PyErr_SetString( (PyObject *)self, "can't set exp_z" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_exp_nspsytune__doc__[] =
"Use Naoki's psycho acoustic model instead of GPSYCHO (experimental!).\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_exp_nspsytune()\n"
;

static PyObject *
mp3enc_set_exp_nspsytune(self, args)
    Encoder *self;
    PyObject *args;
{
    int nspsytune;

    if ( !PyArg_ParseTuple( args, "i", &nspsytune ) )
        return NULL;

    if ( 0 > lame_set_exp_nspsytune( self->gfp, nspsytune ) ) {
        PyErr_SetString( (PyObject *)self, "can't use nspsytune" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_vbr__doc__[] =
"Set VBR mode.\n"
"Default: lame.VBR_OFF (disabled)\n"
"Parameter: lame.VBR_OFF, lame.VBR_OLD, lame.VBR_ABR, lame.VBR_NEW,\n"
"           lame.VBR_DEFAULT\n"
"C function: lame_set_VBR()\n"
;

static PyObject *
mp3enc_set_vbr(self, args)
    Encoder *self;
    PyObject *args;
{
    int vbr;

    if ( !PyArg_ParseTuple( args, "i", &vbr ) )
        return NULL;

    if ( 0 > lame_set_VBR( self->gfp, vbr ) ) {
        PyErr_SetString( (PyObject *)self, "can't set VBR mode" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_vbr_quality__doc__[] =
"Set VBR quality level.\n"
"Default: 4\n"
"Parameter: 0 (highest) - 9 (lowest)\n"
"C function: lame_set_VBR_q()\n"
;

static PyObject *
mp3enc_set_vbr_quality(self, args)
    Encoder *self;
    PyObject *args;
{
    int vbr_quality;

    if ( !PyArg_ParseTuple( args, "i", &vbr_quality ) )
        return NULL;

    if ( 0 > lame_set_VBR_q( self->gfp, vbr_quality ) ) {
        PyErr_SetString( (PyObject *)self, "can't set VBR quality level" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_abr_bitrate__doc__[] =
"Set ABR bitrate.\n"
"Default: 128\n"
"Parameter: int\n"
"C function: lame_set_VBR_mean_bitrate_kbps()\n"
;

static PyObject *
mp3enc_set_abr_bitrate(self, args)
    Encoder *self;
    PyObject *args;
{
    int abr_bitrate;

    if ( !PyArg_ParseTuple( args, "i", &abr_bitrate ) )
        return NULL;

    if ( 0 > lame_set_VBR_mean_bitrate_kbps( self->gfp, abr_bitrate ) ) {
        PyErr_SetString( (PyObject *)self, "can't set ABR bitrate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_vbr_min_bitrate__doc__[] =
"Set minimal bitrate for ABR/VBR.\n"
"Default: lowest possible bitrate\n"
"Parameter: int\n"
"C function: lame_set_VBR_min_bitrate_kbps()\n"
;

static PyObject *
mp3enc_set_vbr_min_bitrate(self, args)
    Encoder *self;
    PyObject *args;
{
    int vbr_min_bitrate;

    if ( !PyArg_ParseTuple( args, "i", &vbr_min_bitrate ) )
        return NULL;

    if ( 0 > lame_set_VBR_min_bitrate_kbps( self->gfp, vbr_min_bitrate ) ) {
        PyErr_SetString( (PyObject *)self, "can't set minimum bitrate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_vbr_max_bitrate__doc__[] =
"Set maximal bitrate for ABR/VBR.\n"
"Default: highest possible bitrate\n"
"Parameter: int\n"
"C function: lame_set_VBR_max_bitrate_kbps()\n"
;

static PyObject *
mp3enc_set_vbr_max_bitrate(self, args)
    Encoder *self;
    PyObject *args;
{
    int vbr_max_bitrate;

    if ( !PyArg_ParseTuple( args, "i", &vbr_max_bitrate ) )
        return NULL;

    if ( 0 > lame_set_VBR_max_bitrate_kbps( self->gfp, vbr_max_bitrate ) ) {
        PyErr_SetString( (PyObject *)self, "can't set maximal bitrate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_vbr_min_enforce__doc__[] =
"Enforce minimum bitrate for ABR/VBR, normally it will be violated for\n"
"analog silence.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_VBR_hard_min()\n"
;

static PyObject *
mp3enc_set_vbr_min_enforce(self, args)
    Encoder *self;
    PyObject *args;
{
    int vbr_min_enforce;

    if ( !PyArg_ParseTuple( args, "i", &vbr_min_enforce ) )
        return NULL;

    if ( 0 > lame_set_VBR_hard_min( self->gfp, vbr_min_enforce ) ) {
        PyErr_SetString( (PyObject *)self, "can't enforce minimal bitrate" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_lowpass_frequency__doc__[] =
"Lowpass frequency in Hz (-1: disabled).\n"
"Default: 0 (LAME chooses)\n"
"Parameter: int\n"
"C function: lame_set_lowpassfreq()\n"
;

static PyObject *
mp3enc_set_lowpass_frequency(self, args)
    Encoder *self;
    PyObject *args;
{
    int lowpass_frequency;

    if ( !PyArg_ParseTuple( args, "i", &lowpass_frequency ) )
        return NULL;

    if ( 0 > lame_set_lowpassfreq( self->gfp, lowpass_frequency ) ) {
        PyErr_SetString( (PyObject *)self, "can't set lowpass frequency" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_lowpass_width__doc__[] =
"Width of transition band in Hz.\n"
"Default: one polyphase filter band\n"
"Parameter: int\n"
"C function: lame_set_lowpasswidth()\n"
;

static PyObject *
mp3enc_set_lowpass_width(self, args)
    Encoder *self;
    PyObject *args;
{
    int lowpass_width;

    if ( !PyArg_ParseTuple( args, "i", &lowpass_width ) )
        return NULL;

    if ( 0 > lame_set_lowpasswidth( self->gfp, lowpass_width ) ) {
        PyErr_SetString( (PyObject *)self, "can't set width of lowpass" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_highpass_frequency__doc__[] =
"Highpass frequency in Hz (-1: disabled).\n"
"Default: 0 (LAME chooses)\n"
"Parameter: int\n"
"C function: lame_set_highpassfreq()\n"
;

static PyObject *
mp3enc_set_highpass_frequency(self, args)
    Encoder *self;
    PyObject *args;
{
    int highpass_frequency;

    if ( !PyArg_ParseTuple( args, "i", &highpass_frequency ) )
        return NULL;

    if ( 0 > lame_set_highpassfreq( self->gfp, highpass_frequency ) ) {
        PyErr_SetString( (PyObject *)self, "can't set highpass frequency" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_highpass_width__doc__[] =
"Set width of highpass in Hz.\n"
"Default: one polyphase filter band\n"
"Parameter: int\n"
"C function: lame_set_highpasswidth()\n"
;

static PyObject *
mp3enc_set_highpass_width(self, args)
    Encoder *self;
    PyObject *args;
{
    int highpass_width;

    if ( !PyArg_ParseTuple( args, "i", &highpass_width ) )
        return NULL;

    if ( 0 > lame_set_highpasswidth( self->gfp, highpass_width ) ) {
        PyErr_SetString( (PyObject *)self, "can't set highpass width" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_ath_for_masking_only__doc__[] =
"Only use ATH for masking.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_ATHonly()\n"
;

static PyObject *
mp3enc_set_ath_for_masking_only(self, args)
    Encoder *self;
    PyObject *args;
{
    int ath_for_masking_only;

    if ( !PyArg_ParseTuple( args, "i", &ath_for_masking_only) )
        return NULL;

    if ( 0 > lame_set_ATHonly( self->gfp, ath_for_masking_only ) ) {
        PyErr_SetString( (PyObject *)self, "can't set ath_for_masking_only" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_ath_for_short_only__doc__[] =
"Only use ATH for short blocks.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_ATHshort()\n"
;

static PyObject *
mp3enc_set_ath_for_short_only(self, args)
    Encoder *self;
    PyObject *args;
{
    int ath_for_short_only;

    if ( !PyArg_ParseTuple( args, "i", &ath_for_short_only ) )
        return NULL;

    if ( 0 > lame_set_ATHshort( self->gfp, ath_for_short_only ) ) {
        PyErr_SetString( (PyObject *)self, "can't set ath_for_short_only" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_ath_disable__doc__[] =
"Disable ATH.\n"
"Default: 0 (use ATH)\n"
"Parameter: int\n"
"C function: lame_set_noATH()\n"
;

static PyObject *
mp3enc_set_ath_disable(self, args)
    Encoder *self;
    PyObject *args;
{
    int ath_disable;

    if ( !PyArg_ParseTuple( args, "i", &ath_disable ) )
        return NULL;

    if ( 0 > lame_set_noATH( self->gfp, ath_disable ) ) {
        PyErr_SetString( (PyObject *)self, "can't disable ATH" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_ath_type__doc__[] =
"Select type of ATH.\n"
"Default: XXX\n"
"Parameter: int  XXX should be lame.YYY\n"
"C function: lame_set_ATHtype()\n"
;

static PyObject *
mp3enc_set_ath_type(self, args)
    Encoder *self;
    PyObject *args;
{
    int ath_type;

    if ( !PyArg_ParseTuple( args, "i", &ath_type ) )
        return NULL;

    if ( 0 > lame_set_ATHtype( self->gfp, ath_type ) ) {
        PyErr_SetString( (PyObject *)self, "can't set ATH type" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_ath_lower__doc__[] =
"Lower ATH by this many dB.\n"
"Default: 0\n"
"Parameter: int\n"
"C function: lame_set_ATHlower()\n"
;

static PyObject *
mp3enc_set_ath_lower(self, args)
    Encoder *self;
    PyObject *args;
{
    int ath_lower;

    if ( !PyArg_ParseTuple( args, "i", &ath_lower ) )
        return NULL;

    if ( 0 > lame_set_ATHlower( self->gfp, ath_lower ) ) {
        PyErr_SetString( (PyObject *)self, "can't lower ATH" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_athaa_type__doc__[] =
"Select type of ATH adaptive adjustment.\n"
"Default: XXX\n"
"Parameter: int  XXX should be lame.YYY\n"
"C function: lame_set_athaa_type()\n"
;

static PyObject *
mp3enc_set_athaa_type(self, args)
    Encoder *self;
    PyObject *args;
{
    int athaa_type;

    if ( !PyArg_ParseTuple( args, "i", &athaa_type ) )
        return NULL;

    if ( 0 > lame_set_athaa_type( self->gfp, athaa_type ) ) {
        PyErr_SetString( (PyObject *)self, "can't select type of ATH adaptive adjustment" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_athaa_loudness_approximation__doc__[] =
"Choose the loudness approximation used by the ATH adaptive auto leveling.\n"
"Default: XXX\n"
"Parameter: int  XXX should be lame.YYY\n"
"C function: lame_set_athaa_loudapprox()\n"
;

static PyObject *
mp3enc_set_athaa_loudness_approximation(self, args)
    Encoder *self;
    PyObject *args;
{
    int athaa_loudness_approximation;

    if ( !PyArg_ParseTuple( args, "i", &athaa_loudness_approximation ) )
        return NULL;

    if ( 0 > lame_set_athaa_loudapprox( self->gfp, athaa_loudness_approximation ) ) {
        PyErr_SetString( (PyObject *)self, "can't set loudness approximation" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_athaa_sensitivity__doc__[] =
"Adjust the point below which adaptive ATH level adjustment occurs (in dB).\n"
"Default: 0\n"
"Parameter: int\n"
"C function: lame_set_athaa_sensitivity()\n"
;

static PyObject *
mp3enc_set_athaa_sensitivity(self, args)
    Encoder *self;
    PyObject *args;
{
    int athaa_sensitivity;

    if ( !PyArg_ParseTuple( args, "i", &athaa_sensitivity ) )
        return NULL;

    if ( 0 > lame_set_athaa_sensitivity( self->gfp, athaa_sensitivity ) ) {
        PyErr_SetString( (PyObject *)self, "can't set ATHaa sensitivity" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_allow_blocktype_difference__doc__[] =
"Allow blocktypes to differ between channels.\n"
"Default: 0 (disabled) for JOINT_STEREO, 1 (enabled) for STEREO\n"
"Parameter: int\n"
"C function: lame_set_allow_diff_short()\n"
;

static PyObject *
mp3enc_set_allow_blocktype_difference(self, args)
    Encoder *self;
    PyObject *args;
{
    int allow_blocktype_difference;

    if ( !PyArg_ParseTuple( args, "i", &allow_blocktype_difference ) )
        return NULL;

    if ( 0 > lame_set_allow_diff_short( self->gfp, allow_blocktype_difference ) ) {
        PyErr_SetString( (PyObject *)self, "can't set allow_blocktype_difference" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_use_temporal_masking__doc__[] =
"Use temporal masking.\n"
"Default: 1 (enabled)\n"
"Parameter: int\n"
"C function: lame_set_useTemporal()\n"
;

static PyObject *
mp3enc_set_use_temporal_masking(self, args)
    Encoder *self;
    PyObject *args;
{
    int use_temporal_masking;

    if ( !PyArg_ParseTuple( args, "i", &use_temporal_masking ) )
        return NULL;

    if ( 0 > lame_set_useTemporal( self->gfp, use_temporal_masking ) ) {
        PyErr_SetString( (PyObject *)self, "can't change temporal masking" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_inter_channel_ratio__doc__[] =
"Set inter channel ratio.\n"
"Default: XXX\n"
"Parameter: float\n"
"C function: lame_set_interChRatio()\n"
;

static PyObject *
mp3enc_set_inter_channel_ratio(self, args)
    Encoder *self;
    PyObject *args;
{
    float inter_channel_ratio;

    if ( !PyArg_ParseTuple( args, "f", &inter_channel_ratio ) )
        return NULL;

    if ( 0 > lame_set_interChRatio( self->gfp, inter_channel_ratio ) ) {
        PyErr_SetString( (PyObject *)self, "can't change inter channel ratio" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_no_short_blocks__doc__[] =
"Disable the use of short blocks.\n"
"Default: 0 (use short blocks)\n"
"Parameter: int\n"
"C function: lame_set_no_short_blocks()\n"
;

static PyObject *
mp3enc_set_no_short_blocks(self, args)
    Encoder *self;
    PyObject *args;
{
    int no_short_blocks;

    if ( !PyArg_ParseTuple( args, "i", &no_short_blocks ) )
        return NULL;

    if ( 0 > lame_set_no_short_blocks( self->gfp, no_short_blocks ) ) {
        PyErr_SetString( (PyObject *)self, "can't change the use of short blocks" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_set_force_short_blocks__doc__[] =
"Force the use of short blocks.\n"
"Default: 0 (disabled)\n"
"Parameter: int\n"
"C function: lame_set_force_short_blocks()\n"
;

static PyObject *
mp3enc_set_force_short_blocks(self, args)
    Encoder *self;
    PyObject *args;
{
    int force_short_blocks;

    if ( !PyArg_ParseTuple( args, "i", &force_short_blocks ) )
        return NULL;

    if ( 0 > lame_set_force_short_blocks( self->gfp, force_short_blocks ) ) {
        PyErr_SetString( (PyObject *)self, "can't force short blocks" );
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_get_frame_num__doc__[] =
"Get number of encoded frames so far.\n"
"C function: lame_get_frameNum()"
;

static PyObject *
mp3enc_get_frame_num(Encoder *self, PyObject *args)
{
    int frame_num;

    frame_num = lame_get_frameNum(self->gfp);

    return Py_BuildValue("i", frame_num);
}


static char mp3enc_get_total_frames__doc__[] =
"Get estimate of the total number of frames to be encoded, only valid if\n"
"the calling program set num_samples.\n"
"C function: lame_get_totalframes()\n"
;

static PyObject *
mp3enc_get_total_frames(Encoder *self, PyObject *args)
{
    int total_frames;

    total_frames = lame_get_totalframes(self->gfp);

    return Py_BuildValue("i", total_frames);
}


static char mp3enc_get_bitrate_histogram__doc__[] =
"Get tuple of histogram dictionaries with bitrate/value keys.\n"
"C functions: lame_bitrate_kbps(), lame_bitrate_hist()\n"
;

static PyObject *
mp3enc_get_bitrate_histogram(Encoder *self, PyObject *args)
{
    int bitrate_count[14];
    int bitrate_value[14];

    lame_bitrate_kbps( self->gfp, bitrate_value );
    lame_bitrate_hist( self->gfp, bitrate_count );

    return Py_BuildValue( "({sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi}{sisi})",
			  "bitrate", bitrate_value[ 0],
			  "value"  , bitrate_count[ 0],
			  "bitrate", bitrate_value[ 1],
			  "value"  , bitrate_count[ 1],
			  "bitrate", bitrate_value[ 2],
			  "value"  , bitrate_count[ 2],
			  "bitrate", bitrate_value[ 3],
			  "value"  , bitrate_count[ 3],
			  "bitrate", bitrate_value[ 4],
			  "value"  , bitrate_count[ 4],
			  "bitrate", bitrate_value[ 5],
			  "value"  , bitrate_count[ 5],
			  "bitrate", bitrate_value[ 6],
			  "value"  , bitrate_count[ 6],
			  "bitrate", bitrate_value[ 7],
			  "value"  , bitrate_count[ 7],
			  "bitrate", bitrate_value[ 8],
			  "value"  , bitrate_count[ 8],
			  "bitrate", bitrate_value[ 9],
			  "value"  , bitrate_count[ 9],
			  "bitrate", bitrate_value[10],
			  "value"  , bitrate_count[10],
			  "bitrate", bitrate_value[11],
			  "value"  , bitrate_count[11],
			  "bitrate", bitrate_value[12],
			  "value"  , bitrate_count[12],
			  "bitrate", bitrate_value[13],
			  "value"  , bitrate_count[13] );
}


static char mp3enc_get_bitrate_values__doc__[] =
"Get tuple of possible bitrates.\n"
"C function: lame_bitrate_kbps()\n"
;

static PyObject *
mp3enc_get_bitrate_values(Encoder *self, PyObject *args)
{
    int bitrate_count[14];

    lame_bitrate_kbps( self->gfp, bitrate_count );

    return Py_BuildValue( "(iiiiiiiiiiiiii)",
			  bitrate_count[ 0],
			  bitrate_count[ 1],
			  bitrate_count[ 2],
			  bitrate_count[ 3],
			  bitrate_count[ 4],
			  bitrate_count[ 5],
			  bitrate_count[ 6],
			  bitrate_count[ 7],
			  bitrate_count[ 8],
			  bitrate_count[ 9],
			  bitrate_count[10],
			  bitrate_count[11],
			  bitrate_count[12],
			  bitrate_count[13] );
}


static char mp3enc_get_bitrate_stereo_mode_histogram__doc__[] =
"Get tuple of dictionaries for stereo mode histogram with LR/LR-I/MS/MS-I keys.\n"
"C function: lame_bitrate_stereo_mode_hist()\n"
;

static PyObject *
mp3enc_get_bitrate_stereo_mode_histogram(Encoder *self, PyObject *args)
{
    int bitrate_stmode_count[14][4];

    lame_bitrate_stereo_mode_hist( self->gfp, bitrate_stmode_count );

    return Py_BuildValue( "({sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi}{sisisisi})",
			  "LR"  , bitrate_stmode_count[ 0][0],
			  "LR-I", bitrate_stmode_count[ 0][1],
			  "MS"  , bitrate_stmode_count[ 0][2],
			  "MS-I", bitrate_stmode_count[ 0][3],
			  "LR"  , bitrate_stmode_count[ 1][0],
			  "LR-I", bitrate_stmode_count[ 1][1],
			  "MS"  , bitrate_stmode_count[ 1][2],
			  "MS-I", bitrate_stmode_count[ 1][3],
			  "LR"  , bitrate_stmode_count[ 2][0],
			  "LR-I", bitrate_stmode_count[ 2][1],
			  "MS"  , bitrate_stmode_count[ 2][2],
			  "MS-I", bitrate_stmode_count[ 2][3],
			  "LR"  , bitrate_stmode_count[ 3][0],
			  "LR-I", bitrate_stmode_count[ 3][1],
			  "MS"  , bitrate_stmode_count[ 3][2],
			  "MS-I", bitrate_stmode_count[ 3][3],
			  "LR"  , bitrate_stmode_count[ 4][0],
			  "LR-I", bitrate_stmode_count[ 4][1],
			  "MS"  , bitrate_stmode_count[ 4][2],
			  "MS-I", bitrate_stmode_count[ 4][3],
			  "LR"  , bitrate_stmode_count[ 5][0],
			  "LR-I", bitrate_stmode_count[ 5][1],
			  "MS"  , bitrate_stmode_count[ 5][2],
			  "MS-I", bitrate_stmode_count[ 5][3],
			  "LR"  , bitrate_stmode_count[ 6][0],
			  "LR-I", bitrate_stmode_count[ 6][1],
			  "MS"  , bitrate_stmode_count[ 6][2],
			  "MS-I", bitrate_stmode_count[ 6][3],
			  "LR"  , bitrate_stmode_count[ 7][0],
			  "LR-I", bitrate_stmode_count[ 7][1],
			  "MS"  , bitrate_stmode_count[ 7][2],
			  "MS-I", bitrate_stmode_count[ 7][3],
			  "LR"  , bitrate_stmode_count[ 8][0],
			  "LR-I", bitrate_stmode_count[ 8][1],
			  "MS"  , bitrate_stmode_count[ 8][2],
			  "MS-I", bitrate_stmode_count[ 8][3],
			  "LR"  , bitrate_stmode_count[ 9][0],
			  "LR-I", bitrate_stmode_count[ 9][1],
			  "MS"  , bitrate_stmode_count[ 9][2],
			  "MS-I", bitrate_stmode_count[ 9][3],
			  "LR"  , bitrate_stmode_count[10][0],
			  "LR-I", bitrate_stmode_count[10][1],
			  "MS"  , bitrate_stmode_count[10][2],
			  "MS-I", bitrate_stmode_count[10][3],
			  "LR"  , bitrate_stmode_count[11][0],
			  "LR-I", bitrate_stmode_count[11][1],
			  "MS"  , bitrate_stmode_count[11][2],
			  "MS-I", bitrate_stmode_count[11][3],
			  "LR"  , bitrate_stmode_count[12][0],
			  "LR-I", bitrate_stmode_count[12][1],
			  "MS"  , bitrate_stmode_count[12][2],
			  "MS-I", bitrate_stmode_count[12][3],
			  "LR"  , bitrate_stmode_count[13][0],
			  "LR-I", bitrate_stmode_count[13][1],
			  "MS"  , bitrate_stmode_count[13][2],
			  "MS-I", bitrate_stmode_count[13][3] );
}


static char mp3enc_get_stereo_mode_histogram__doc__[] =
"Get dictionary for stereo mode histogram with LR/LR-I/MS/MS-I keys.\n"
"C function: lame_stereo_mode_hist()\n"
;

static PyObject *
mp3enc_get_stereo_mode_histogram(Encoder *self, PyObject *args)
{
    int stmode_count[4];

    lame_stereo_mode_hist( self->gfp, stmode_count );

    return Py_BuildValue( "{sisisisi}",
			  "LR"  , stmode_count[0],
			  "LR-I", stmode_count[1],
			  "MS"  , stmode_count[2],
			  "MS-I", stmode_count[3] );
}


static char mp3enc_set_exp_msfix__doc__[] =
"Undocumented, don't use.\n"
"Parameter: double\n"
"C function: lame_set_msfix()\n"
;

static PyObject *
mp3enc_set_exp_msfix(self, args)
    Encoder *self;
    PyObject *args;
{
    double msfix;

    if ( !PyArg_ParseTuple( args, "d", &msfix ) )
        return NULL;

    lame_set_msfix( self->gfp, msfix );

    Py_INCREF(Py_None);
    return Py_None;
}


static char mp3enc_write_tags__doc__[] =
"Write ID3v1 TAG's.\n"
"Parameter: file\n"
"C function: lame_mp3_tags_fid()\n"
;

static PyObject *
mp3enc_write_tags(self, args)
    Encoder *self;
    PyObject *args;
{
    PyObject *object;
    FILE *mp3_file;

    if ( !PyArg_ParseTuple( args, "O", &object ) )
        return NULL;

    if ( 0 == PyFile_Check( object ) )
	return NULL;

    mp3_file = PyFile_AsFile( object );

    lame_mp3_tags_fid( self->gfp, mp3_file );

    Py_INCREF(Py_None);
    return Py_None;
}


static struct PyMethodDef mp3enc_methods[] = {
    {"encode_interleaved", (PyCFunction)mp3enc_encode_interleaved,
        METH_VARARGS, mp3enc_encode_interleaved__doc__               },
    {"flush_buffers", (PyCFunction)mp3enc_flush_buffers,
        METH_NOARGS, mp3enc_flush_buffers__doc__},
    {"init", (PyCFunction)mp3enc_init,
        METH_NOARGS, mp3enc_init__doc__},
    {"set_num_samples", (PyCFunction)mp3enc_set_num_samples,
	METH_VARARGS, mp3enc_set_num_samples__doc__                  },
    {"set_out_samplerate", (PyCFunction)mp3enc_set_out_samplerate,
	METH_VARARGS, mp3enc_set_out_samplerate__doc__               },
    {"set_analysis", (PyCFunction)mp3enc_set_analysis,
	METH_VARARGS, mp3enc_set_analysis__doc__                     },
    {"set_write_vbr_tag", (PyCFunction)mp3enc_set_write_vbr_tag,
	METH_VARARGS, mp3enc_set_write_vbr_tag__doc__                },
    {"set_quality", (PyCFunction)mp3enc_set_quality,
	METH_VARARGS, mp3enc_set_quality__doc__                       },
    {"set_mode", (PyCFunction)mp3enc_set_mode,
	METH_VARARGS, mp3enc_set_mode__doc__                          },
    {"set_force_ms", (PyCFunction)mp3enc_set_force_ms,
	METH_VARARGS, mp3enc_set_force_ms__doc__                      },
    {"set_free_format", (PyCFunction)mp3enc_set_free_format,
	METH_VARARGS, mp3enc_set_free_format__doc__                   },
    {"set_bitrate", (PyCFunction)mp3enc_set_bitrate,
	METH_VARARGS, mp3enc_set_bitrate__doc__                       },
    {"set_compression_ratio", (PyCFunction)mp3enc_set_compression_ratio,
	METH_VARARGS, mp3enc_set_compression_ratio__doc__             },
    {"set_preset", (PyCFunction)mp3enc_set_preset,
	METH_VARARGS, mp3enc_set_preset__doc__                        },
    {"set_asm_optimizations", (PyCFunction)mp3enc_set_asm_optimizations,
	METH_VARARGS, mp3enc_set_asm_optimizations__doc__             },
    {"set_copyright", (PyCFunction)mp3enc_set_copyright,
	METH_VARARGS, mp3enc_set_copyright__doc__                     },
    {"set_original", (PyCFunction)mp3enc_set_original,
	METH_VARARGS, mp3enc_set_original__doc__                      },
    {"set_error_protection", (PyCFunction)mp3enc_set_error_protection,
	METH_VARARGS, mp3enc_set_error_protection__doc__              },
    {"set_extension", (PyCFunction)mp3enc_set_extension,
	METH_VARARGS, mp3enc_set_extension__doc__                     },
    {"set_strict_iso", (PyCFunction)mp3enc_set_strict_iso,
	METH_VARARGS, mp3enc_set_strict_iso__doc__                    },
    {"set_disable_reservoir", (PyCFunction)mp3enc_set_disable_reservoir,
	METH_VARARGS, mp3enc_set_disable_reservoir__doc__             },
    {"set_exp_quantization", (PyCFunction)mp3enc_set_exp_quantization,
	METH_VARARGS, mp3enc_set_exp_quantization__doc__              },
    {"set_exp_y", (PyCFunction)mp3enc_set_exp_y,
	METH_VARARGS, mp3enc_set_exp_y__doc__                         },
    {"set_exp_z", (PyCFunction)mp3enc_set_exp_z,
	METH_VARARGS, mp3enc_set_exp_z__doc__                         },
    {"set_exp_nspsytune", (PyCFunction)mp3enc_set_exp_nspsytune,
	METH_VARARGS, mp3enc_set_exp_nspsytune__doc__                 },
    {"set_vbr", (PyCFunction)mp3enc_set_vbr,
	METH_VARARGS, mp3enc_set_vbr__doc__                           },
    {"set_vbr_quality", (PyCFunction)mp3enc_set_vbr_quality,
	METH_VARARGS, mp3enc_set_vbr_quality__doc__                   },
    {"set_abr_bitrate", (PyCFunction)mp3enc_set_abr_bitrate,
	METH_VARARGS, mp3enc_set_abr_bitrate__doc__                   },
    {"set_vbr_min_bitrate", (PyCFunction)mp3enc_set_vbr_min_bitrate,
	METH_VARARGS, mp3enc_set_vbr_min_bitrate__doc__               },
    {"set_vbr_max_bitrate", (PyCFunction)mp3enc_set_vbr_max_bitrate,
	METH_VARARGS, mp3enc_set_vbr_max_bitrate__doc__               },
    {"set_vbr_min_enforce", (PyCFunction)mp3enc_set_vbr_min_enforce,
	METH_VARARGS, mp3enc_set_vbr_min_enforce__doc__               },
    {"set_lowpass_frequency", (PyCFunction)mp3enc_set_lowpass_frequency,
	METH_VARARGS, mp3enc_set_lowpass_frequency__doc__             },
    {"set_lowpass_width", (PyCFunction)mp3enc_set_lowpass_width,
	METH_VARARGS, mp3enc_set_lowpass_width__doc__                 },
    {"set_highpass_frequency", (PyCFunction)mp3enc_set_highpass_frequency,
	METH_VARARGS, mp3enc_set_highpass_frequency__doc__            },
    {"set_highpass_width", (PyCFunction)mp3enc_set_highpass_width,
	METH_VARARGS, mp3enc_set_highpass_width__doc__                },
    {"set_ath_for_masking_only", (PyCFunction)mp3enc_set_ath_for_masking_only,
	METH_VARARGS, mp3enc_set_ath_for_masking_only__doc__          },
    {"set_ath_for_short_only", (PyCFunction)mp3enc_set_ath_for_short_only,
	METH_VARARGS, mp3enc_set_ath_for_short_only__doc__            },
    {"set_ath_disable", (PyCFunction)mp3enc_set_ath_disable,
	METH_VARARGS, mp3enc_set_ath_disable__doc__                   },
    {"set_ath_type", (PyCFunction)mp3enc_set_ath_type,
	METH_VARARGS, mp3enc_set_ath_type__doc__                      },
    {"set_ath_lower", (PyCFunction)mp3enc_set_ath_lower,
	METH_VARARGS, mp3enc_set_ath_lower__doc__                     },
    {"set_athaa_type", (PyCFunction)mp3enc_set_athaa_type,
	METH_VARARGS, mp3enc_set_athaa_type__doc__                    },
    {"set_athaa_loudness_approximation", (PyCFunction)mp3enc_set_athaa_loudness_approximation,
	METH_VARARGS, mp3enc_set_athaa_loudness_approximation__doc__  },
    {"set_athaa_sensitivity", (PyCFunction)mp3enc_set_athaa_sensitivity,
	METH_VARARGS, mp3enc_set_athaa_sensitivity__doc__             },
    {"set_allow_blocktype_difference", (PyCFunction)mp3enc_set_allow_blocktype_difference,
	METH_VARARGS, mp3enc_set_allow_blocktype_difference__doc__    },
    {"set_use_temporal_masking", (PyCFunction)mp3enc_set_use_temporal_masking,
	METH_VARARGS, mp3enc_set_use_temporal_masking__doc__          },
    {"set_inter_channel_ratio", (PyCFunction)mp3enc_set_inter_channel_ratio,
	METH_VARARGS, mp3enc_set_inter_channel_ratio__doc__           },
    {"set_no_short_blocks", (PyCFunction)mp3enc_set_no_short_blocks,
	METH_VARARGS, mp3enc_set_no_short_blocks__doc__               },
    {"set_force_short_blocks", (PyCFunction)mp3enc_set_force_short_blocks,
	METH_VARARGS, mp3enc_set_force_short_blocks__doc__            },
    {"get_frame_num", (PyCFunction)mp3enc_get_frame_num,
        METH_NOARGS, mp3enc_get_frame_num__doc__},
    {"get_total_frames", (PyCFunction)mp3enc_get_total_frames,
        METH_NOARGS, mp3enc_get_total_frames__doc__},
    {"get_bitrate_histogram", (PyCFunction)mp3enc_get_bitrate_histogram,
        METH_NOARGS, mp3enc_get_bitrate_histogram__doc__},
    {"get_bitrate_values", (PyCFunction)mp3enc_get_bitrate_values,
        METH_NOARGS, mp3enc_get_bitrate_values__doc__},
    {"get_bitrate_stereo_mode_histogram", (PyCFunction)mp3enc_get_bitrate_stereo_mode_histogram,
        METH_NOARGS, mp3enc_get_bitrate_stereo_mode_histogram__doc__ },
    {"get_stereo_mode_histogram", (PyCFunction)mp3enc_get_stereo_mode_histogram,
        METH_NOARGS, mp3enc_get_stereo_mode_histogram__doc__},
    {"set_exp_msfix", (PyCFunction)mp3enc_set_exp_msfix,
	METH_VARARGS, mp3enc_set_exp_msfix__doc__            },
    {"write_tags", (PyCFunction)mp3enc_write_tags,
	METH_VARARGS, mp3enc_write_tags__doc__                        },
    {NULL,    NULL}  /* Sentinel */
};

/* "Generic" setters for int and float values of the lame encoder that
take care of the argument verification needed when dealing with
attributes. */

static int
generic_set_int(Encoder *self, PyObject *value, const char *attr,
                int (fptr)(lame_global_flags*, int))
{
    if (value == NULL) {
        PyErr_Format(PyExc_TypeError, "Cannot delete the '%s' attribute.", attr);
        return -1;
    }

    if (!PyInt_Check(value)) {
        PyErr_Format(PyExc_TypeError, "Attribute '%s' must be an integer.", attr);
        return -1;
    }

    if (0 != (fptr)(self->gfp, PyInt_AS_LONG(value))) {
        PyErr_Format(PyExc_ValueError, "Set '%s' failed (out of range?).", attr);
        return -1;
    }

    return 0;
}

static int
generic_set_float(Encoder *self, PyObject *value, const char *attr,
                int (fptr)(lame_global_flags*, float))
{
    if (value == NULL) {
        PyErr_Format(PyExc_TypeError, "Cannot delete the '%s' attribute.", attr);
        return -1;
    }

    if (!PyFloat_Check(value)) {
        PyErr_Format(PyExc_TypeError, "Attribute '%s' must be a float.", attr);
        return -1;
    }

    if (0 != (fptr)(self->gfp, PyFloat_AS_DOUBLE(value))) {
        PyErr_Format(PyExc_ValueError, "Set '%s' failed (out of range?).", attr);
        return -1;
    }

    return 0;
}


/* Macros to ease creation of attribute getter/setter wrapper
 * functions.  I Know this is a bit much, but you can use "gcc -E" to
 * see what it generates, and it would be truly tedious without.
 */
#define GETATTR(attrname, format) \
    static PyObject *\
    mp3enc_get_##attrname(Encoder *self, void *closure) { \
        return Py_BuildValue(#format, lame_get_##attrname(self->gfp)); \
    }

#define SETATTR_INT(attrname, lamefunc) \
    static int \
    mp3enc_set_##attrname(Encoder *self, PyObject *value, void *closure) { \
        return generic_set_int(self, value, #attrname, lamefunc); \
    }

#define SETATTR_FLOAT(attrname, lamefunc) \
    static int \
    mp3enc_set_##attrname(Encoder *self, PyObject *value, void *closure) { \
        return generic_set_float(self, value, #attrname, lamefunc); \
    }


GETATTR(in_samplerate, i)
SETATTR_INT(in_samplerate, lame_set_in_samplerate)
GETATTR(num_channels, i)
SETATTR_INT(num_channels, lame_set_num_channels)

GETATTR(scale, f)
SETATTR_FLOAT(scale, lame_set_scale)
GETATTR(scale_left, f)
SETATTR_FLOAT(scale_left, lame_set_scale_left)
GETATTR(scale_right, f)
SETATTR_FLOAT(scale_right, lame_set_scale_right)

static PyGetSetDef mp3enc_getseters[] = {
    {"in_samplerate",
        (getter)mp3enc_get_in_samplerate, (setter)mp3enc_set_in_samplerate,
    "Input sample rate in Hz.", NULL},
    {"num_channels",
        (getter)mp3enc_get_num_channels, (setter)mp3enc_set_num_channels,
    "Number of channels in the input stream.", NULL},
    {"scale",
        (getter)mp3enc_get_scale, (setter)mp3enc_set_scale,
    "Scale the input by this amount before encoding.", NULL},
    {"scale_left",
        (getter)mp3enc_get_scale_left, (setter)mp3enc_set_scale_left,
    "Scale channel 0 (left) of the input by this amount before encoding.", NULL},
    {"scale_right",
        (getter)mp3enc_get_scale_right, (setter)mp3enc_set_scale_right,
    "Scale channel 1 (right) of the input by this amount before encoding.", NULL},
    {NULL} /* Sentinel */
};

/* Encoder type declaration */
static PyTypeObject EncoderType = {
        PyObject_HEAD_INIT(NULL)
        0,                              /* ob_size */
        "lame.encoder",                 /* tp_name */
        sizeof(Encoder),                /* tp_basicsize */
        0,                              /* tp_itemsize */
        /* methods */
        (destructor)mp3enc_dealloc,     /* tp_dealloc */
        0,                              /* tp_print */
        0,                              /* tp_getattr */
        0,                              /* tp_setattr */
        0,                              /* tp_compare */
        0,                              /* tp_repr */
        0,                              /* tp_as_number */
        0,                              /* tp_as_sequence */
        0,                              /* tp_as_mapping */
        0,                              /* tp_hashg */
        0,                              /* tp_call */
        0,                              /* tp_str */
        0,                              /* tp_getattro */
        0,                              /* tp_setattro */
        0,                              /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "Encoder object.",              /* tp_doc */
        0,                              /* tp_traverse */
        0,                              /* tp_clear */
        0,                              /* tp_richcompare */
        0,                              /* tp_weaklistoffset */
        0,                              /* tp_iter */
        0,                              /* tp_iternext */
        mp3enc_methods,                 /* TODO tp_methods */
        0,                              /* tp_members */
        mp3enc_getseters,               /* tp_getset */
        0,                              /* tp_base */
        0,                              /* tp_dict */
        0,                              /* tp_descr_get */
        0,                              /* tp_descr_set */
        0,                              /* tp_dictoffset */
        0,                              /* tp_init */
        0,                              /* tp_alloc */
        mp3enc_new,                     /* tp_new */
};


/* BEGIN lame module functions */


static char mp3lame_version__doc__[] =
"Returns the version of LAME in a tuple: (major, minor, alpha, beta,\n"
"psy_major, sy_minor, psy_alpha, psy_beta, compile_time_features)"
;

static PyObject *
mp3lame_version(PyObject *self, PyObject *args)
{
    lame_version_t version;

    get_lame_version_numerical( &version );

    return Py_BuildValue("iiiiiiiis",
                         version.major,
                         version.minor,
                         version.alpha,
                         version.beta,
                         version.psy_major,
                         version.psy_minor,
                         version.psy_alpha,
                         version.psy_beta,
                         version.features);
}


/* END lame module functions. */

/* List of methods defined in the module */

static struct PyMethodDef mp3lame_methods[] = {
    {"version", (PyCFunction)mp3lame_version, METH_NOARGS, mp3lame_version__doc__},
    {NULL}  /* Sentinel */
};

/* Initialization function for the module (*must* be called initlame) */

static char lame_module_documentation[] =
"Python module for the LAME encoder."
;

PyMODINIT_FUNC
init_lame()
{
    PyObject *m;

    if (PyType_Ready(&EncoderType) < 0)
        return;

    /* Create the module and add the functions */
    m = Py_InitModule3("_lame", mp3lame_methods, lame_module_documentation);
    if (NULL == m)
        return;

    /* Register the lame.encoder object type */
    Py_INCREF(&EncoderType);
    PyModule_AddObject(m, "encoder", (PyObject *)&EncoderType);

    /* Add some symbolic constants to the module */
    /* String version constants for convenience. */
    PyModule_AddStringConstant(m, "LAME_VERSION",
                               (char *)get_lame_version());
    PyModule_AddStringConstant(m, "LAME_VERSION_SHORT",
                               (char *)get_lame_short_version());
    PyModule_AddStringConstant(m, "LAME_VERSION_VERY_SHORT",
                               (char *)get_lame_very_short_version());
    PyModule_AddStringConstant(m, "LAME_URL",
                               (char *)get_lame_url());

    /* Expose presets */
    PyModule_AddIntConstant(m, "PRESET_ABR_8", ABR_8);
    PyModule_AddIntConstant(m, "PRESET_ABR_320", ABR_320);
    PyModule_AddIntConstant(m, "PRESET_VBR_9", V9);
    PyModule_AddIntConstant(m, "PRESET_VBR_8", V8);
    PyModule_AddIntConstant(m, "PRESET_VBR_7", V7);
    PyModule_AddIntConstant(m, "PRESET_VBR_6", V6);
    PyModule_AddIntConstant(m, "PRESET_VBR_5", V5);
    PyModule_AddIntConstant(m, "PRESET_VBR_4", V4);
    PyModule_AddIntConstant(m, "PRESET_VBR_3", V3);
    PyModule_AddIntConstant(m, "PRESET_VBR_2", V2);
    PyModule_AddIntConstant(m, "PRESET_VBR_1", V1);
    PyModule_AddIntConstant(m, "PRESET_VBR_0", V0);
    /* Older ones for compatibility */
    PyModule_AddIntConstant(m, "PRESET_R3MIX", R3MIX);
    PyModule_AddIntConstant(m, "PRESET_STANDARD", STANDARD);
    PyModule_AddIntConstant(m, "PRESET_EXTREME", EXTREME);
    PyModule_AddIntConstant(m, "PRESET_INSANE", INSANE);
    PyModule_AddIntConstant(m, "PRESET_STANDARD_FAST", STANDARD_FAST);
    PyModule_AddIntConstant(m, "PRESET_EXTREME_FAST", EXTREME_FAST);
    PyModule_AddIntConstant(m, "PRESET_MEDIUM", MEDIUM);
    PyModule_AddIntConstant(m, "PRESET_MEDIUM_FAST", MEDIUM_FAST);

    /* Expose VBR encoding modes. */
    PyModule_AddIntConstant(m, "VBR_MODE_OFF", vbr_off);
    PyModule_AddIntConstant(m, "VBR_MODE_RH", vbr_rh);
    PyModule_AddIntConstant(m, "VBR_MODE_ABR", vbr_abr);
    PyModule_AddIntConstant(m, "VBR_MODE_MTRH", vbr_mtrh);
    PyModule_AddIntConstant(m, "VBR_MODE_DEFAULT", vbr_default);

    /* TODO Convert these names to better things... */
    PyModule_AddIntConstant(m, "MPEG_MODE_STEREO", STEREO);
    PyModule_AddIntConstant(m, "MPEG_MODE_JOINT_STEREO", JOINT_STEREO);
    PyModule_AddIntConstant(m, "MPEG_MODE_DUAL", DUAL_CHANNEL);
    PyModule_AddIntConstant(m, "MPEG_MODE_MONO", MONO);

    PyModule_AddIntConstant(m, "PAD_NO", PAD_NO);
    PyModule_AddIntConstant(m, "PAD_ALL", PAD_ALL);
    PyModule_AddIntConstant(m, "PAD_ADJUST", PAD_ADJUST);

    PyModule_AddIntConstant(m, "ASM_MMX", MMX);
    PyModule_AddIntConstant(m, "ASM_3DNOW", AMD_3DNOW);
    PyModule_AddIntConstant(m, "ASM_SSE", SSE);

    /* Check for errors */
    if (PyErr_Occurred())
        Py_FatalError("can't initialize module lame");
}
