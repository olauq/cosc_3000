/****************************************************************************/
/* Copyright (c) 2011-2017, Ola Olsson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/****************************************************************************/
#ifndef __OBJModel_h_
#define __OBJModel_h_

#include "GL/glew.h"
#include "GL/glut.h"
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <glm/glm.hpp>
#include "Aabb.h"


class OBJModel
{
public:
	enum RenderFlags
	{
		RF_Opaque = 1, /**< Draw chunks that are fully opaque, i.e. do not have transparency or an opacity map */
		RF_AlphaTested = 1 << 1,  /**< Draw chunks that have an opacity map */
		RF_Transparent = 1 << 2,  /**< Draw chunks that have an alpha value != 1.0f */
		RF_All = RF_Opaque | RF_AlphaTested | RF_Transparent,  /**< Draw everything. */
	};

	OBJModel(void);
	~OBJModel(void);
	/**
	 */
	void render(GLuint shaderProgram, uint32_t renderFlags, const glm::mat4 &viewMatrix);
	void render(GLuint shaderProgram, uint32_t renderFlags = RF_All) { render(shaderProgram, renderFlags, glm::mat4(1.0f)); }
	/**
	 */
	bool load(std::string fileName); 
  /**
   */
  const Aabb &getAabb() const { return m_aabb; }

  // used open GL texture units, ensure they are bound appropriately for the shaders (e.g., setDefaultUniformBindings).
  enum TextureUnits
  {
    TU_Diffuse = 0,
    TU_Opacity,
    TU_Specular,
    TU_Normal,
    TU_Max,
  };
  // used in open gl, ensure they are bound appropriately for the shaders (e.g., using bindDefaultAttributes).
  enum AttributeArrays
  {
    AA_Position = 0,
    AA_Normal,
    AA_TexCoord,
    AA_Tangent,
    AA_Bitangent,
    AA_Max,
  };

  /**
   * Matrial properties are packed into an uniform buffer, should be declared thus (in the shader):
   *  layout(std140) uniform MaterialProperties
   *  {
   *    vec3 material_diffuse_color; 
   *    vec3 material_specular_color; 
   *    vec3 material_emissive_color; 
   *    float material_specular_exponent;
   *  };
   */
  enum UniformBufferSlots
  {
    UBS_MaterialProperties = 0,
    UBS_Max,
  };


	/**
	 * Helper to ensure the attribute arrays provided by ObjModel is assigned to the default names in the shader.
	 * A program can call this to conveniently set these up before linking the shader.
	 */
	static void bindDefaultAttributes(GLuint shaderProgram)
	{
		glBindAttribLocation(shaderProgram, AA_Position, "positionAttribute");
		glBindAttribLocation(shaderProgram, AA_Normal, "normalAttribute");
		glBindAttribLocation(shaderProgram, AA_TexCoord, "texCoordAttribute");
		glBindAttribLocation(shaderProgram, AA_Tangent, "tangentAttribute");
		glBindAttribLocation(shaderProgram, AA_Bitangent, "bitangentAttribute");
	}


	/**
	 * Helper to set the default uniforms provided by ObjModel. This only needs to be done once after creating the shader
	 * NOTE: the shader must be bound when calling this function.
	 */
	static void setDefaultUniformBindings(GLuint shaderProgram)
	{
		GLint id;
		assert((glGetIntegerv(GL_CURRENT_PROGRAM, &id), id == shaderProgram));

		glUniform1i(glGetUniformLocation(shaderProgram, "diffuse_texture"), TU_Diffuse);
		glUniform1i(glGetUniformLocation(shaderProgram, "opacity_texture"), TU_Opacity);
		glUniform1i(glGetUniformLocation(shaderProgram, "specular_texture"), TU_Specular);
		glUniform1i(glGetUniformLocation(shaderProgram, "normal_texture"), TU_Normal);
		glUniformBlockBinding(shaderProgram, glGetUniformBlockIndex(shaderProgram, "MaterialProperties"), UBS_MaterialProperties);
	}

public:

	size_t getNumVerts();

	bool loadOBJ(std::ifstream &file, std::string basePath);
	bool loadMaterials(std::string fileName, std::string basePath);
	unsigned int loadTexture(std::string fileName, std::string basePath, bool srgb);

	struct Material
	{
		struct Color
		{
			glm::vec3 diffuse;
			glm::vec3 ambient;
			glm::vec3 specular;
			glm::vec3 emissive;
		} color;
		float specularExponent;
    struct TextureId
    {
      int diffuse;
      int opacity;
      int specular;
      int normal;
    } textureId;

		float alpha;
    size_t offset;
	};


  typedef std::map<std::string, Material> MatrialMap;
	MatrialMap m_materials;

  // Matches layout of uniform buffer under std140 layout (OpenGL spec.).
  struct MaterialProperties_Std140
  {
    glm::vec3 diffuse_color; 
    float alpha; 
    glm::vec3 specular_color; 
    float pad1; // padding is required to ensure vectors start on multiples of 4
    glm::vec3 emissive_color; 
    float specular_exponent;
    // this meets the alignment required for uniform buffer offsets on NVidia GTX280/480, also 
    // compatible with AMD integrated Radeon HD 3100, and modern Intel ingerated GPUs (16 bytes).
    float alignPad[52]; // Pads struct to 256 bytes, large enough for everyone...
  };
  GLuint m_materialPropertiesBuffer;

	struct Chunk
	{
		Aabb aabb;
		Material *material;
    uint32_t offset;
    uint32_t count;
		uint32_t renderFlags;
	};

  size_t m_numVerts;
	// Data on host
	std::vector<glm::vec3> m_positions;
	std::vector<glm::vec3> m_normals;
	std::vector<glm::vec2> m_uvs; 
	std::vector<glm::vec3> m_tangents;
	std::vector<glm::vec3> m_bitangents;
  // Data on GPU
	GLuint	m_positions_bo; 
	GLuint	m_normals_bo; 
	GLuint	m_uvs_bo; 
	GLuint	m_tangents_bo; 
	GLuint	m_bitangents_bo; 
	// Vertex Array Object
	GLuint	m_vaob;

	std::vector<Chunk> m_chunks;

  Aabb m_aabb;
  GLuint m_defaultTextureOne; /**< all 1, single pixel texture to use when no texture is loaded. */
  GLuint m_defaultNormalTexture;  /**< { 0.5, 0.5, 1, 1 }, single pixel float texture to use when no normal texture is loaded. */

	friend struct SortAlphaChunksPred;

	bool m_overrideDiffuseTextureWithDefault;
};

#endif // __OBJModel_h_
