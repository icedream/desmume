/*
	Copyright (C) 2009-2010 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/


// Author: yolky-nine


#ifndef X432R_BUILDSWITCH_H_INCLUDED
#define X432R_BUILDSWITCH_H_INCLUDED

#include <cmath>
#include <string>
#include <assert.h>
#include "../types.h"


template <typename TYPE>
inline TYPE clamp(const TYPE value, const TYPE min, const TYPE max)
{
	assert(min <= max);
	
	if(value < min) return min;
	if(value > max) return max;
	
	return value;
}


// 高解像度3Dレンダリングを有効化
#define X432R_CUSTOMRENDERER_ENABLED

// タッチ入力関係の変更を有効化
#define X432R_TOUCHINPUT_ENABLED

// メニュー関係の変更を有効化
#define X432R_MENUITEMMOD_ENABLED

// ファイルパス関係の変更を有効化
#define X432R_FILEPATHMOD_ENABLED


namespace X432R
{
	//--- 高解像度3Dレンダリング ---
	#ifdef X432R_CUSTOMRENDERER_ENABLED
		// 3D Renderer
		#define X432R_CUSTOMSOFTRASTENGINE_ENABLED			// ok
		#define X432R_CUSTOMRENDERER_CLEARIMAGE_ENABLED		// not tested
		
		// GPU
//		#define X432R_DISPCAPTURE_MAINMEMORYFIFO_TEST		// not tested
		
		
		//--- experimental ---
		#define X432R_SAMPLEDVRAMDATACHECK_TEST				// temp
		#define X432R_LAYERPOSITIONOFFSET_TEST
		
		#define X432R_OPENGL_FOG_ENABLED
		#define X432R_OPENGL_CUSTOMSTENCILTEST
		
		#define X432R_LOWQUALITYMODE_TEST
		#define X432R_PPL_TEST
		
		#define X432R_D3D_DISPLAYMETHOD_ENABLED
		
		
		#if 0
//		#define X432R_OPENGL_CUSTOMFRAMEBUFFEROPERATION_TEST
//		#define X432R_RENDER3D_BILLBOARDCHECK_TEST
		
//		#define X432R_OPENGL_CUSTOMFRAMEBUFFEROPERATION_TEST2
//		#define X432R_SOFTRAST_OPTIMIZE_TEST
		
		
		//--- incomplete ---
//		#define X432R_LAYERPOSITIONOFFSET_TEST2
//		#define X432R_HIGHRESO_BG_OBJ_ROTSCALE_TEST
		
//		#define X432R_OPENGL_2PASSSHADOW_TEST
		
//		#define X432R_3D_REARPLANE_TEST
		
//		#define X432R_CPP_AMP_TEST
		
		
		//--- debug ---
//		#define X432R_SINGLECORE_TEST
		#define X432R_PROCESSTIME_CHECK
		#define X432R_CUSTOMRENDERER_DEBUG
//		#define X432R_CUSTOMRENDERER_DEBUG2
//		#define X432R_HIGHRESOLUTION_RENDERLINE_DEBUG
		#endif
		
		
		#define X432R_STATIC_RENDER_MAGNIFICATION_CHECK() \
			static_assert( (RENDER_MAGNIFICATION >= 2) && (RENDER_MAGNIFICATION <= 4) , "X432R: invalid rendering magnification" )
		
		
		bool IsHighResolutionRendererSelected();
		bool IsSoftRasterzierSelected();
		u32 GetCurrentRenderMagnification();
		void ClearBuffers();
		
		#ifdef X432R_LOWQUALITYMODE_TEST
		extern bool lowQualityMsaaEnabled;
		extern bool lowQualityAlphaBlendEnabled;
		#endif
		
		#ifdef X432R_PROCESSTIME_CHECK
		class ProcessTimeCounter
		{
			private:
			u32 startTime;
			u32 totalTime;
//			u32 execCount;
			
			public:
			u32 Time;
//			u32 Count;
			
			void Start();
			void Stop();
			void Reset(const u32 fps);
		};
		
		class AutoStopTimeCounter
		{
			private:
			ProcessTimeCounter &timeCounter;
			
			public:
			AutoStopTimeCounter(ProcessTimeCounter& counter) : timeCounter(counter)
			{	timeCounter.Start();		}
			
			~AutoStopTimeCounter()
			{	timeCounter.Stop();			}
		};
		
		extern ProcessTimeCounter timeCounter_3D;
		extern ProcessTimeCounter timeCounter_3DFinish1;
		extern ProcessTimeCounter timeCounter_3DFinish2;
		extern ProcessTimeCounter timeCounter_2D;
		extern ProcessTimeCounter timeCounter_2DHighReso;
		#endif
		
		#ifdef X432R_CUSTOMRENDERER_DEBUG
		extern bool debugModeEnabled;
		extern bool debugModeEnabled2;
		
		void ShowDebugMessage(std::string message);
		#endif
	#endif
	
	
	//--- メニュー ---
	#ifdef X432R_MENUITEMMOD_ENABLED
		
		#define X432R_CUSTOMSCREENSCALING_ENABLED
		
		
		extern bool showMenubarInFullScreen;
		extern bool cpuPowerSavingEnabled;
		
		void HK_ToggleSoundEnabledKeyDown(int, bool);
		void HK_FastForwardKeyUp(int);
		void HK_FastForwardKeyDown(int, bool);
		void HK_ToggleFastForwardKeyDown(int, bool);
		void HK_SlowMotionKeyUp(int);
		void HK_SlowMotionKeyDown(int, bool);
		void HK_ToggleSlowMotionKeyDown(int, bool);
		
		
		#ifdef X432R_CUSTOMSCREENSCALING_ENABLED
		void GetScreenScales(float &mainscreen_scale, float &subscreen_scale);
		float GetSmallScreenOffset_Horizontal();
		float GetSmallScreenOffset_Vertical();
		
		void WritePrivateProfileFloat(const std::string app_name, const std::string key_name, const float value);
		float GetPrivateProfileFloat(const std::string app_name, const std::string key_name, const float default_value);
		float CalculateDisplayScale(const float client_width, const float client_height, const float target_width, const float target_height);
		
		void HK_ChangeSmallScreenSetting(int, const bool decrease_upperscreen_size);
		void HK_SwapSmallScreenSetting(int, bool);
		#endif
	#endif
}

#endif



