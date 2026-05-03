// Disable the warnings about needs to have DLL interface as we have tons of vector templates
#ifdef _MSC_VER
  #pragma warning( disable: 4251 )
#endif

#ifndef ASTER_TOPOLOGYDLLEXPORT
	#ifdef WIN32
		#ifdef ASTER_TOPOLOGYDLL
			#ifdef BUILDASTER_TOPOLOGYDLL
        #define ASTER_TOPOLOGYDLLEXPORT __declspec(dllexport)
        #define ASTER_TOPOLOGYDLLEXPORTONLY __declspec(dllexport)
			#else
        #define ASTER_TOPOLOGYDLLEXPORT __declspec(dllimport)
        #define ASTER_TOPOLOGYDLLEXPORTONLY
			#endif
		#else		
			#define ASTER_TOPOLOGYDLLEXPORT
			#define ASTER_TOPOLOGYDLLEXPORTONLY
		#endif
	#else
		#define ASTER_TOPOLOGYDLLEXPORT
		#define ASTER_TOPOLOGYDLLEXPORTONLY
	#endif
#endif
