
add_executable( TestLocalizer TestLocalizer.cpp )
target_link_libraries( TestLocalizer vrlt_multiview )
target_link_libraries( TestLocalizer vrlt_localizer  )
target_link_libraries( TestLocalizer vrlt_features  )
target_link_libraries( TestLocalizer vrlt_featurematcher  )

find_library( TAG toontag REQUIRED )
add_executable( TestTracker TestTracker.cpp )
target_link_libraries( TestTracker vrlt_multiview  )
target_link_libraries( TestTracker vrlt_patchtracker  )
target_link_libraries( TestTracker ${TAG} )

#add_executable( TestFilter TestFilter.cpp ../KalmanFilter/kalmanfilter.h )
#target_link_libraries( TestFilter vrlt_multiview  )
#target_link_libraries( TestFilter vrlt_patchtracker  )
#target_link_libraries( TestFilter ${TAG} )

find_library( OPENGL OpenGL REQUIRED )
find_library( GLUT GLUT REQUIRED )
add_executable( RenderTrack RenderTrack.cpp )
target_link_libraries( RenderTrack vrlt_multiview  )
target_link_libraries( RenderTrack vrlt_glutils  )
target_link_libraries( RenderTrack ${OPENGL} )
target_link_libraries( RenderTrack ${GLUT} )
