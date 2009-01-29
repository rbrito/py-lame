"""The python inteface to the LAME MP3 encoding library."""

from _lame import *

__all__ = ['ASM_3DNOW', 'ASM_MMX', 'ASM_SSE',
           'LAME_URL', 'LAME_VERSION', 'LAME_VERSION_SHORT',
           'LAME_VERSION_VERY_SHORT',
           'MPEG_MODE_DUAL', 'MPEG_MODE_JOINT_STEREO', 'MPEG_MODE_MONO',
           'MPEG_MODE_STEREO',
           'PAD_ADJUST', 'PAD_ALL', 'PAD_NO',
           'PRESET_ABR_320', 'PRESET_ABR_8', 'PRESET_EXTREME',
           'PRESET_EXTREME_FAST', 'PRESET_INSANE', 'PRESET_MEDIUM',
           'PRESET_MEDIUM_FAST', 'PRESET_R3MIX', 'PRESET_STANDARD',
           'PRESET_STANDARD_FAST', 'PRESET_VBR_0', 'PRESET_VBR_1',
           'PRESET_VBR_2', 'PRESET_VBR_3', 'PRESET_VBR_4', 'PRESET_VBR_5',
           'PRESET_VBR_6', 'PRESET_VBR_7', 'PRESET_VBR_8', 'PRESET_VBR_9',
           'VBR_MODE_ABR', 'VBR_MODE_DEFAULT', 'VBR_MODE_MTRH', 'VBR_MODE_OFF',
           'VBR_MODE_RH',
           'Encoder', 'EncoderError', 'module_version', 'version',
           # Local exports
           'url']

# Pull from C compile time.
__version__ = module_version


def url():
    """Deprecated, use lame.LAME_URL instead."""
    return LAME_URL
