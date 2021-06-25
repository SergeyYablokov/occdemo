#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout (vertices = 1) out;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_CS[];
layout(location = 1) in mediump vec2 aVertexUVs_CS[];
layout(location = 2) in mediump vec3 aVertexNormal_CS[];
layout(location = 3) in mediump vec3 aVertexTangent_CS[];
layout(location = 4) in highp vec3 aVertexShUVs_CS[][4];
#else
in highp vec3 aVertexPos_CS[];
in mediump vec2 aVertexUVs_CS[];
in mediump vec3 aVertexNormal_CS[];
in mediump vec3 aVertexTangent_CS[];
in highp vec3 aVertexShUVs_CS[][4];
#endif

struct OutputPatch {
    vec3 aVertexPos_B030;
    vec3 aVertexPos_B021;
    vec3 aVertexPos_B012;
    vec3 aVertexPos_B003;
    vec3 aVertexPos_B102;
    vec3 aVertexPos_B201;
    vec3 aVertexPos_B300;
    vec3 aVertexPos_B210;
    vec3 aVertexPos_B120;
    vec3 aVertexPos_B111;
	vec2 aVertexUVs[3];
    vec3 aVertexNormal[3];
	vec3 aVertexTangent[3];
    //vec3 aVertexShUVs[3][4];
};

layout(location = 0) out patch OutputPatch oPatch;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

float GetTessLevel(float Distance0, float Distance1) {
    float AvgDistance = (Distance0 + Distance1) / 1.0;
	return min((2.0 * 1024.0) / (AvgDistance * AvgDistance), 8.0);
}

vec3 ProjectToPlane(vec3 Point, vec3 PlanePoint, vec3 PlaneNormal) {
    vec3 v = Point - PlanePoint;
    float Len = dot(v, PlaneNormal);
    vec3 d = Len * PlaneNormal;
    return (Point - d);
}

void CalcPositions() {
    // The original vertices stay the same
    oPatch.aVertexPos_B030 = aVertexPos_CS[0];
    oPatch.aVertexPos_B003 = aVertexPos_CS[1];
    oPatch.aVertexPos_B300 = aVertexPos_CS[2];

    // Edges are names according to the opposing vertex
    vec3 EdgeB300 = oPatch.aVertexPos_B003 - oPatch.aVertexPos_B030;
    vec3 EdgeB030 = oPatch.aVertexPos_B300 - oPatch.aVertexPos_B003;
    vec3 EdgeB003 = oPatch.aVertexPos_B030 - oPatch.aVertexPos_B300;

    // Generate two midpoints on each edge
    oPatch.aVertexPos_B021 = oPatch.aVertexPos_B030 + EdgeB300 / 3.0;
    oPatch.aVertexPos_B012 = oPatch.aVertexPos_B030 + EdgeB300 * 2.0 / 3.0;
    oPatch.aVertexPos_B102 = oPatch.aVertexPos_B003 + EdgeB030 / 3.0;
    oPatch.aVertexPos_B201 = oPatch.aVertexPos_B003 + EdgeB030 * 2.0 / 3.0;
    oPatch.aVertexPos_B210 = oPatch.aVertexPos_B300 + EdgeB003 / 3.0;
    oPatch.aVertexPos_B120 = oPatch.aVertexPos_B300 + EdgeB003 * 2.0 / 3.0;

    // Project each midpoint on the plane defined by the nearest vertex and its normal
    oPatch.aVertexPos_B021 = ProjectToPlane(oPatch.aVertexPos_B021, oPatch.aVertexPos_B030,
                                            oPatch.aVertexNormal[0]);
    oPatch.aVertexPos_B012 = ProjectToPlane(oPatch.aVertexPos_B012, oPatch.aVertexPos_B003,
                                            oPatch.aVertexNormal[1]);
    oPatch.aVertexPos_B102 = ProjectToPlane(oPatch.aVertexPos_B102, oPatch.aVertexPos_B003,
                                            oPatch.aVertexNormal[1]);
    oPatch.aVertexPos_B201 = ProjectToPlane(oPatch.aVertexPos_B201, oPatch.aVertexPos_B300,
                                            oPatch.aVertexNormal[2]);
    oPatch.aVertexPos_B210 = ProjectToPlane(oPatch.aVertexPos_B210, oPatch.aVertexPos_B300,
                                            oPatch.aVertexNormal[2]);
    oPatch.aVertexPos_B120 = ProjectToPlane(oPatch.aVertexPos_B120, oPatch.aVertexPos_B030,
                                            oPatch.aVertexNormal[0]);

    // Handle the center
    vec3 Center = (oPatch.aVertexPos_B003 + oPatch.aVertexPos_B030 + oPatch.aVertexPos_B300) / 3.0;
    oPatch.aVertexPos_B111 = (oPatch.aVertexPos_B021 + oPatch.aVertexPos_B012 + oPatch.aVertexPos_B102 +
                          oPatch.aVertexPos_B201 + oPatch.aVertexPos_B210 + oPatch.aVertexPos_B120) / 6.0;
    oPatch.aVertexPos_B111 += (oPatch.aVertexPos_B111 - Center) / 2.0;
}

void main(void) {
	for (int i = 0; i < 3; i++) {
		oPatch.aVertexUVs[i] = aVertexUVs_CS[i];
		oPatch.aVertexNormal[i] = aVertexNormal_CS[i];
		oPatch.aVertexTangent[i] = aVertexTangent_CS[i];
		//oPatch.aVertexShUVs[i][0] = aVertexShUVs_CS[i][0];
		//oPatch.aVertexShUVs[i][1] = aVertexShUVs_CS[i][1];
		//oPatch.aVertexShUVs[i][2] = aVertexShUVs_CS[i][2];
		//oPatch.aVertexShUVs[i][3] = aVertexShUVs_CS[i][3];
	}
	
	CalcPositions();
	
	float EyeToVertexDistance0 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_CS[0]);
    float EyeToVertexDistance1 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_CS[1]);
    float EyeToVertexDistance2 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_CS[2]);
	
	gl_TessLevelOuter[0] = GetTessLevel(EyeToVertexDistance1, EyeToVertexDistance2);
	gl_TessLevelOuter[1] = GetTessLevel(EyeToVertexDistance2, EyeToVertexDistance0);
	gl_TessLevelOuter[2] = GetTessLevel(EyeToVertexDistance0, EyeToVertexDistance1);
	gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
} 
