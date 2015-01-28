/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/** \file DNA_sequence_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 *
 * Structs for use by the 'Sequencer' (Video Editor)
 *
 * Note on terminology
 * - #Sequence: video/effect/audio data you can select and manipulate in the sequencer.
 * - #Sequence.machine: Strange name for the channel.
 * - #Strip: The data referenced by the #Sequence
 * - Meta Strip (SEQ_TYPE_META): Support for nesting Sequences.
 */

#ifndef __DNA_SEQUENCE_TYPES_H__
#define __DNA_SEQUENCE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_color_types.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

struct Ipo;
struct Scene;
struct bSound;
struct MovieClip;

/* strlens; 256= FILE_MAXFILE, 768= FILE_MAXDIR */

typedef struct StripElem {
	char name[256];
	int orig_width, orig_height;
} StripElem;

typedef struct StripCrop {
	int top;
	int bottom;
	int left;
	int right;
} StripCrop;

typedef struct StripTransform {
	int xofs;
	int yofs;
} StripTransform;

typedef struct StripColorBalance {
	float lift[3];
	float gamma[3];
	float gain[3];
	int flag;
	int pad;
	// float exposure;
	// float saturation;
} StripColorBalance;

typedef struct StripProxy {
	char dir[768];         // custom directory for index and proxy files
	                       // (defaults to BL_proxy)

	char file[256];        // custom file
	struct anim *anim;     // custom proxy anim file

	short tc;              // time code in use

	short quality;         // proxy build quality
	short build_size_flags; // size flags (see below) of all proxies
	                       // to build
	short build_tc_flags;  // time code flags (see below) of all tc indices
	                       // to build
	short build_flags;
	char pad[6];
} StripProxy;

typedef struct Strip {
	struct Strip *next, *prev;
	int us, done;
	int startstill, endstill;
	StripElem *stripdata;  /* only used as an array in IMAGE sequences(!),
	                        * and as a 1-element array in MOVIE sequences,
	                        * NULL for all other strip-types */
	char dir[768];
	StripProxy *proxy;
	StripCrop *crop;
	StripTransform *transform;
	StripColorBalance *color_balance DNA_DEPRECATED;

	/* color management */
	ColorManagedColorspaceSettings colorspace_settings;
} Strip;

/**
 * The sequence structure is the basic struct used by any strip.
 * each of the strips uses a different sequence structure.
 *
 * \warning The first part identical to ID (for use in ipo's)
 * the commend above is historic, probably we can drop the ID compatibility,
 * but take care making this change.
 *
 * \warning This is really a 'Strip' in the UI!, name is highly confusing.
 */
typedef struct Sequence {
	struct Sequence *next, *prev;
	void *tmp; /* tmp var for copying, and tagging for linked selection */
	void *lib; /* needed (to be like ipo), else it will raise libdata warnings, this should never be used */
	char name[64]; /* SEQ_NAME_MAXSTR - name, set by default and needs to be unique, for RNA paths */

	int flag, type; /*flags bitmap (see below) and the type of sequence*/
	int len; /* the length of the contents of this strip - before handles are applied */
	int start; /* start frame of contents of strip in absolute frame coordinates. For metastrips start of first strip startdisp */
	int startofs, endofs; /* frames after the first frame where display starts, frames before the last frame where display ends */
	int startstill, endstill; /* frames that use the first frame before data begins, frames that use the last frame after data ends */
	int machine, depth; /*machine - the strip channel, depth - the depth in the sequence when dealing with metastrips */
	int startdisp, enddisp; /* starting and ending points of the strip in the sequence*/
	float sat;
	float mul, handsize;

	short anim_preseek;
	short streamindex;    /* streamindex for movie or sound files with several streams */
	int multicam_source;  /* for multicam source selection */
	int clip_flag;        /* MOVIECLIP render flags */

	Strip *strip;

	struct Ipo *ipo DNA_DEPRECATED;   /* old animation system, deprecated for 2.5 */

	/* these ID vars should never be NULL but can be when linked libs fail to load, so check on access */
	struct Scene     *scene;
	struct Object    *scene_camera;  /* override scene camera */
	struct MovieClip *clip;          /* for MOVIECLIP strips */
	struct Mask      *mask;          /* for MASK strips */

	struct anim *anim;      /* for MOVIE strips */

	float effect_fader;
	float speed_fader;

	/* pointers for effects: */
	struct Sequence *seq1, *seq2, *seq3;
	
	ListBase seqbase;       /* list of strips for metastrips */

	struct bSound *sound;   /* the linked "bSound" object */
	void *scene_sound;
	float volume;

	float pitch, pan;     /* pitch (-0.1..10), pan -2..2 */
	float strobe;

	void *effectdata;     /* Struct pointer for effect settings */

	int anim_startofs;    /* only use part of animation file */
	int anim_endofs;      /* is subtle different to startofs / endofs */


	int blend_mode;
	float blend_opacity;

	/* is sfra needed anymore? - it looks like its only used in one place */
	int sfra;  /* starting frame according to the timeline of the scene. */

	char alpha_mode;
	char pad[3];
	
	/* modifiers */
	ListBase modifiers;
} Sequence;

typedef struct MetaStack {
	struct MetaStack *next, *prev;
	ListBase *oldbasep;
	Sequence *parseq;
	/* the startdisp/enddisp when entering the meta */
	int disp_range[2];
} MetaStack;

typedef struct Editing {

	/* old data format */		
	ListBase *seqbasep; /* pointer to the current list of seq's being edited (can be within a meta strip) */
	ListBase seqbase;   /* pointer to the top-most seq's */
	ListBase metastack;
	
	/* Context vars, used to be static */
	Sequence *act_seq;

	/* new data */
	ListBase nstripbase;
	
	char act_imagedir[1024]; /* 1024 = FILE_MAX */
	char act_sounddir[1024]; /* 1024 = FILE_MAX */

	int over_ofs, over_cfra;
	int over_flag, pad;
	rctf over_border;
} Editing;

/* ************* Effect Variable Structs ********* */
typedef struct WipeVars {
	float edgeWidth, angle;
	short forward, wipetype;
} WipeVars;

typedef struct GlowVars {	
	float fMini;    /*	Minimum intensity to trigger a glow */
	float fClamp;
	float fBoost;   /*	Amount to multiply glow intensity */
	float dDist;    /*	Radius of glow blurring */
	int dQuality;
	int bNoComp;    /*	SHOW/HIDE glow buffer */
} GlowVars;

typedef struct TransformVars {
	float ScalexIni;
	float ScaleyIni;
	float xIni;
	float yIni;
	float rotIni;
	int percent;
	int interpolation;
	int uniform_scale; /* preserve aspect/ratio when scaling */
} TransformVars;

typedef struct SolidColorVars {
	float col[3];
	float pad;
} SolidColorVars;

typedef struct SpeedControlVars {
	float *frameMap;
	float globalSpeed;
	int flags;
	int length;
	int lastValidFrame;
} SpeedControlVars;

typedef struct GaussianBlurVars {
	float size_x;
	float size_y;
} GaussianBlurVars;

/* ***************** Sequence modifiers ****************** */

typedef struct SequenceModifierData {
	struct SequenceModifierData *next, *prev;
	int type, flag;
	char name[64]; /* MAX_NAME */

	/* mask input, either sequence or maks ID */
	int mask_input_type, pad;

	struct Sequence *mask_sequence;
	struct Mask     *mask_id;
} SequenceModifierData;

typedef struct ColorBalanceModifierData {
	SequenceModifierData modifier;

	StripColorBalance color_balance;
	float color_multiply;
} ColorBalanceModifierData;

typedef struct CurvesModifierData {
	SequenceModifierData modifier;

	struct CurveMapping curve_mapping;
} CurvesModifierData;

typedef struct HueCorrectModifierData {
	SequenceModifierData modifier;

	struct CurveMapping curve_mapping;
} HueCorrectModifierData;

typedef struct BrightContrastModifierData {
	SequenceModifierData modifier;

	float bright;
	float contrast;
} BrightContrastModifierData;

typedef struct SequencerMaskModifierData {
	SequenceModifierData modifier;
} SequencerMaskModifierData;

/* ***************** Scopes ****************** */

typedef struct SequencerScopes {
	struct ImBuf *reference_ibuf;

	struct ImBuf *zebra_ibuf;
	struct ImBuf *waveform_ibuf;
	struct ImBuf *sep_waveform_ibuf;
	struct ImBuf *vector_ibuf;
	struct ImBuf *histogram_ibuf;
} SequencerScopes;

#define MAXSEQ          32

#define SELECT 1

/* Editor->over_flag */
#define SEQ_EDIT_OVERLAY_SHOW           1
#define SEQ_EDIT_OVERLAY_ABS            2

#define SEQ_STRIP_OFSBOTTOM     0.2f
#define SEQ_STRIP_OFSTOP        0.8f

/* SpeedControlVars->flags */
#define SEQ_SPEED_INTEGRATE      1
/* #define SEQ_SPEED_BLEND          2 */ /* DEPRECATED */
#define SEQ_SPEED_COMPRESS_IPO_Y 4

/* ***************** SEQUENCE ****************** */
#define SEQ_NAME_MAXSTR         64

/* seq->flag */
enum {
	SEQ_LEFTSEL                 = (1 << 1),
	SEQ_RIGHTSEL                = (1 << 2),
	SEQ_OVERLAP                 = (1 << 3),
	SEQ_FILTERY                 = (1 << 4),
	SEQ_MUTE                    = (1 << 5),
	SEQ_MAKE_PREMUL             = (1 << 6), /* deprecated, used for compatibility code only */
	SEQ_REVERSE_FRAMES          = (1 << 7),
	SEQ_IPO_FRAME_LOCKED        = (1 << 8),
	SEQ_EFFECT_NOT_LOADED       = (1 << 9),
	SEQ_FLAG_DELETE             = (1 << 10),
	SEQ_FLIPX                   = (1 << 11),
	SEQ_FLIPY                   = (1 << 12),
	SEQ_MAKE_FLOAT              = (1 << 13),
	SEQ_LOCK                    = (1 << 14),
	SEQ_USE_PROXY               = (1 << 15),
	SEQ_USE_TRANSFORM           = (1 << 16),
	SEQ_USE_CROP                = (1 << 17),
	/* SEQ_USE_COLOR_BALANCE       = (1 << 18), */ /* DEPRECATED */
	SEQ_USE_PROXY_CUSTOM_DIR    = (1 << 19),

	SEQ_USE_PROXY_CUSTOM_FILE   = (1 << 21),
	SEQ_USE_EFFECT_DEFAULT_FADE = (1 << 22),
	SEQ_USE_LINEAR_MODIFIERS    = (1 << 23),

	/* flags for whether those properties are animated or not */
	SEQ_AUDIO_VOLUME_ANIMATED   = (1 << 24),
	SEQ_AUDIO_PITCH_ANIMATED    = (1 << 25),
	SEQ_AUDIO_PAN_ANIMATED      = (1 << 26),
	SEQ_AUDIO_DRAW_WAVEFORM     = (1 << 27),
	
	/* don't include Grease Pencil in OpenGL previews of Scene strips */
	SEQ_SCENE_NO_GPENCIL        = (1 << 28),
	/* access scene strips directly (like a metastrip) */
	SEQ_SCENE_STRIPS            = (1 << 29),

	SEQ_INVALID_EFFECT          = (1 << 31),
};

#if (DNA_DEPRECATED_GCC_POISON == 1)
#pragma GCC poison SEQ_MAKE_PREMUL
#endif

/* convenience define for all selection flags */
#define SEQ_ALLSEL  (SELECT + SEQ_LEFTSEL + SEQ_RIGHTSEL)

/* deprecated, don't use a flag anymore*/
/*#define SEQ_ACTIVE                            1048576*/

#define SEQ_COLOR_BALANCE_INVERSE_GAIN 1
#define SEQ_COLOR_BALANCE_INVERSE_GAMMA 2
#define SEQ_COLOR_BALANCE_INVERSE_LIFT 4

/* !!! has to be same as IMB_imbuf.h IMB_PROXY_... and IMB_TC_... */

#define SEQ_PROXY_IMAGE_SIZE_25                 1
#define SEQ_PROXY_IMAGE_SIZE_50                 2
#define SEQ_PROXY_IMAGE_SIZE_75                 4
#define SEQ_PROXY_IMAGE_SIZE_100                8

#define SEQ_PROXY_TC_NONE                       0
#define SEQ_PROXY_TC_RECORD_RUN                 1
#define SEQ_PROXY_TC_FREE_RUN                   2
#define SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN   4
#define SEQ_PROXY_TC_RECORD_RUN_NO_GAPS         8
#define SEQ_PROXY_TC_ALL                        15

/* SeqProxy->build_flags */
enum {
	SEQ_PROXY_SKIP_EXISTING = 1,
};

/* seq->alpha_mode */
enum {
	SEQ_ALPHA_STRAIGHT = 0,
	SEQ_ALPHA_PREMUL   = 1
};

/* seq->type WATCH IT: SEQ_TYPE_EFFECT BIT is used to determine if this is an effect strip!!! */
enum {
	SEQ_TYPE_IMAGE       = 0,
	SEQ_TYPE_META        = 1,
	SEQ_TYPE_SCENE       = 2,
	SEQ_TYPE_MOVIE       = 3,
	SEQ_TYPE_SOUND_RAM   = 4,
	SEQ_TYPE_SOUND_HD    = 5,
	SEQ_TYPE_MOVIECLIP   = 6,
	SEQ_TYPE_MASK        = 7,

	SEQ_TYPE_EFFECT      = 8,
	SEQ_TYPE_CROSS       = 8,
	SEQ_TYPE_ADD         = 9,
	SEQ_TYPE_SUB         = 10,
	SEQ_TYPE_ALPHAOVER   = 11,
	SEQ_TYPE_ALPHAUNDER  = 12,
	SEQ_TYPE_GAMCROSS    = 13,
	SEQ_TYPE_MUL         = 14,
	SEQ_TYPE_OVERDROP    = 15,
	/* SEQ_TYPE_PLUGIN      = 24, */ /* Deprecated */
	SEQ_TYPE_WIPE        = 25,
	SEQ_TYPE_GLOW        = 26,
	SEQ_TYPE_TRANSFORM   = 27,
	SEQ_TYPE_COLOR       = 28,
	SEQ_TYPE_SPEED       = 29,
	SEQ_TYPE_MULTICAM    = 30,
	SEQ_TYPE_ADJUSTMENT  = 31,
	SEQ_TYPE_GAUSSIAN_BLUR = 40,
	SEQ_TYPE_EFFECT_MAX  = 40
};

#define SEQ_MOVIECLIP_RENDER_UNDISTORTED (1 << 0)
#define SEQ_MOVIECLIP_RENDER_STABILIZED  (1 << 1)

#define SEQ_BLEND_REPLACE      0
/* all other BLEND_MODEs are simple SEQ_TYPE_EFFECT ids and therefore identical
 * to the table above. (Only those effects that handle _exactly_ two inputs,
 * otherwise, you can't really blend, right :) !)
 */


#define SEQ_HAS_PATH(_seq) (ELEM((_seq)->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD))

/* modifiers */

/* SequenceModifierData->type */
enum {
	seqModifierType_ColorBalance   = 1,
	seqModifierType_Curves         = 2,
	seqModifierType_HueCorrect     = 3,
	seqModifierType_BrightContrast = 4,
	seqModifierType_Mask           = 5,

	NUM_SEQUENCE_MODIFIER_TYPES
};

/* SequenceModifierData->flag */
enum {
	SEQUENCE_MODIFIER_MUTE      = (1 << 0),
	SEQUENCE_MODIFIER_EXPANDED  = (1 << 1),
};

enum {
	SEQUENCE_MASK_INPUT_STRIP   = 0,
	SEQUENCE_MASK_INPUT_ID      = 1
};

/************************* NEW DATA TYPES **********************************/

#define STRIP_NAME_MAXSTR         64

/* nstrip->type */
enum {
	NSTRIP_TYPE_DATA = 1,
	NSTRIP_TYPE_FX,
	NSTRIP_TYPE_CONTAINER,	
};

/* nstrip->classtype */
enum {
	NSTRIP_CLASS_MOVIE = 1,
	NSTRIP_CLASS_MOVIECLIP,
	NSTRIP_CLASS_MASK,
	NSTRIP_CLASS_IMAGE,
	NSTRIP_CLASS_SOUND,
	NSTRIP_CLASS_SCENE,
	NSTRIP_CLASS_EFFECT,
	NSTRIP_CLASS_CROSS,
	NSTRIP_CLASS_ADD,
	NSTRIP_CLASS_SUB,
	NSTRIP_CLASS_ALPHAOVER,
	NSTRIP_CLASS_ALPHAUNDER,
	NSTRIP_CLASS_GAMCROSS,
	NSTRIP_CLASS_MUL,
	NSTRIP_CLASS_OVERDROP,
	NSTRIP_CLASS_WIPE,
	NSTRIP_CLASS_GLOW,
	NSTRIP_CLASS_TRANSFORM,
	NSTRIP_CLASS_COLOR,
	NSTRIP_CLASS_SPEED,
	NSTRIP_CLASS_MULTICAM,
	NSTRIP_CLASS_ADJUSTMENT,
	NSTRIP_CLASS_GAUSSIAN_BLUR,
};

/* nclip->flag */
enum {
	NSTRIP_LEFTSEL                 = (1 << 1),
	NSTRIP_RIGHTSEL                = (1 << 2),
	NSTRIP_OVERLAP                 = (1 << 3),
	NSTRIP_FILTERY                 = (1 << 4),
	NSTRIP_MUTE                    = (1 << 5),
	NSTRIP_REVERSE_FRAMES          = (1 << 6),
	NSTRIP_IPO_FRAME_LOCKED        = (1 << 7),
	NSTRIP_EFFECT_NOT_LOADED       = (1 << 8),
	NSTRIP_FLAG_DELETE             = (1 << 9),
	NSTRIP_FLIPX                   = (1 << 10),
	NSTRIP_FLIPY                   = (1 << 11),
	NSTRIP_MAKE_FLOAT              = (1 << 12),
	NSTRIP_LOCK                    = (1 << 13),
	NSTRIP_USE_PROXY               = (1 << 14),
	NSTRIP_USE_TRANSFORM           = (1 << 15),
	NSTRIP_USE_CROP                = (1 << 16),
	NSTRIP_USE_PROXY_CUSTOM_DIR    = (1 << 17),

	NSTRIP_USE_PROXY_CUSTOM_FILE   = (1 << 18),
	NSTRIP_USE_EFFECT_DEFAULT_FADE = (1 << 19),
	NSTRIP_USE_LINEAR_MODIFIERS    = (1 << 20),

	/* flags for whether those properties are animated or not */
	NSTRIP_AUDIO_VOLUME_ANIMATED   = (1 << 21),
	NSTRIP_AUDIO_PITCH_ANIMATED    = (1 << 22),
	NSTRIP_AUDIO_PAN_ANIMATED      = (1 << 23),
	NSTRIP_AUDIO_DRAW_WAVEFORM     = (1 << 24),

	NCLIP_INVALID_EFFECT          = (1 << 25),
};
/* This is the base class and it's strictly related to the visual clip in the sequencer */
typedef struct NStrip {
	struct NStrip *next, *prev;
	char name[64]; /* STRIP_NAME_MAXSTR - name, set by default and needs to be unique, for RNA paths */
	
	/* general use flags */
	int flag;

	/* the track this clip exists in (used to be "machine" in the old code) */
	int track;
	
	/* depth in the sequence (how many levels of clip inclusion deep the strip is) */
	int depth;

	/* main type, data or fx */
	short type;
	
	/* classtype identifier, to quickly determine the type of strip */
	short classtype;
	
	/* frame position in the timeline */
	int start;
	int end;
	
	/* attachments point of clip. if the clip moves, the attachments do too */
	ListBase attachments;
} NStrip;

/* a data clip, it includes movies, sounds or image sequences */
typedef struct NDataStrip {
	NStrip clip;

	/* length of source data (depends on source data type) */
	int len;
	
	/* offset of data from start of the clip */
	int offset;
} NDataStrip;

/* a data clip, it includes movies, sounds or image sequences */
typedef struct NFXStrip {
	NStrip clip;
	
	/* fader of the effect */
	float effect_fader;
	float pad;
	
	/* specialize those per fx */
	struct NStrip *clip1, *clip2, *clip3;	
} NFXStrip;

/* old metastrip, a clip that contains other clips */
typedef struct NContainerStrip {
	NStrip clip;
	
	/* list of contained clips */
	ListBase clips;
} NContainerStrip;

typedef struct NMovieStrip {
	NDataStrip data;
	
	/* render flags */
	int render_flag;

	int blend_mode;
		
	/* saturation */
	float saturation;
	
	float blend_opacity;	
	
	/* streamindex for sound files with several streams */
	short streamindex;
	
	/* animation movie preseek */
	short anim_preseek;

	int pad;
	
	/* modifiers */
	ListBase modifiers;
} NMovieStrip;

typedef struct NAnimStrip {
	NMovieStrip movie;
	
	/* for amination file */
	struct anim *anim;	
} NAnimStrip;

typedef struct NTrackerStrip {
	NMovieStrip movie;
	
	/* source MovieClip strip */
	struct MovieClip *clip;
} NTrackerStrip;

typedef struct NMaskStrip {
	NDataStrip data;
	
	/* source mask */
	struct Mask *mask;	
} NMaskStrip;

/* sound clip */
typedef struct NSoundStrip {
	NDataStrip data;
	
	/* streamindex for sound files with several streams */
	int streamindex;
	
	/* sound volume */
	float volume;

	/* pitch (-0.1..10) */
	float pitch;
	
	/* pan -2..2 */
	float pan;
	
	/* the linked "bSound" object */
	struct bSound *sound;
} NSoundStrip;

/* scene strip */
typedef struct NSceneStrip {
	NDataStrip data;
	
	/* the linked scene */
	struct Scene     *scene;

	/* override of scene camera */
	struct Object *scene_camera;	
} NSceneStrip;



#endif  /* __DNA_SEQUENCE_TYPES_H__ */
