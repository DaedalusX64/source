add_library(HLEGraphics_Vita OBJECT 
                           Combiner/BlendConstant.cpp
                           Combiner/CombinerExpression.cpp
                           Combiner/CombinerTree.cpp
                           Combiner/RenderSettings.cpp
                           BlendModes.cpp
                           ColourAdjuster.cpp
                           GraphicsPluginVita.cpp
                           RendererLegacy.cpp
                           RendererModern.cpp

            )

if(DAEDALUS_PROFILE_HLEGRAPHICS_VITA
    target_compile_options(Debug_PSP PRIVATE -pg)
endif(DAEDALUS_PROFILE_HLEGRAPHICS_VITA)
            
list(APPEND daed_libs "HLEGraphics_Vita")
set(daed_libs ${daed_libs} PARENT_SCOPE)
