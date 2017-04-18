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
#include "Aabb.h"
#include <float.h>
#include <algorithm>

// Helper to extract translation from GLM matrix, it is the fourth column (we're assuming that no projection has taken place, because then we'd need to divide by w).
inline glm::vec3 getTranslationAffine(const glm::mat4 &m)
{
	return glm::vec3(m[3]);
}


Aabb make_aabb(const glm::vec3 &min, const glm::vec3 &max)
{
  Aabb result = { min, max };
  return result;
}


Aabb make_inverse_extreme_aabb()
{
  return make_aabb(glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX), glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
}



Aabb make_aabb(const glm::vec3 *positions, const size_t numPositions)
{
  Aabb result = make_inverse_extreme_aabb();

  for (size_t i = 0; i < numPositions; ++i)
  {
    result = combine(result, positions[i]);
  }

  return result;
}


Aabb operator - (const Aabb &a, const glm::vec3 &offset)
{
	Aabb r = a;
	r.min -= offset;
	r.max -= offset;
	return r;
}



Aabb operator + (const Aabb &a, const glm::vec3 &offset)
{
	Aabb r = a;
	r.min += offset;
	r.max += offset;
	return r;
}


