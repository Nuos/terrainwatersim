#include "PCH.h"
#include "UniformBuffer.h"
#include "../GLUtils.h"
#include "../ShaderObject.h"

namespace gl
{
  UniformBuffer* UniformBuffer::s_pBoundUBOs[16];

  UniformBuffer::UniformBuffer() :
    m_BufferObject(9999),
    m_uiBufferSizeBytes(0),
    m_Variables(),
    m_sBufferName(""),
    m_pBufferData(NULL),
    m_uiBufferDirtyRangeEnd(0),
    m_uiBufferDirtyRangeStart(0)
  {
  }

  UniformBuffer::~UniformBuffer(void)
  {
    EZ_DEFAULT_DELETE(m_pBufferData);
    glDeleteBuffers(1, &m_BufferObject);
  }

  ezResult UniformBuffer::Init(ezUInt32 uiBufferSizeBytes, const ezString& sBufferName)
  {
    m_sBufferName = sBufferName;
    m_uiBufferSizeBytes = uiBufferSizeBytes;

    EZ_DEFAULT_DELETE(m_pBufferData);
    m_pBufferData = EZ_DEFAULT_NEW_RAW_BUFFER(ezInt8, uiBufferSizeBytes);
    m_uiBufferDirtyRangeEnd = m_uiBufferDirtyRangeStart = 0;

    glGenBuffers(1, &m_BufferObject);
    glBindBuffer(GL_UNIFORM_BUFFER, m_BufferObject);
    // glBufferStorage(GL_UNIFORM_BUFFER, m_uiBufferSizeBytes, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    glBufferData(GL_UNIFORM_BUFFER, m_uiBufferSizeBytes, NULL, GL_DYNAMIC_DRAW);

    return gl::Utils::CheckError("glBufferData");
  }

  ezResult UniformBuffer::Init(const gl::ShaderObject& shader, const ezString& sBufferName)
  {
    gl::UniformBufferMetaInfo* uniformBufferInfo = nullptr;
    if (!shader.GetUniformBufferInfo().TryGetValue(sBufferName, uniformBufferInfo))
    {
      ezLog::Error("Shader \"%s\" doesn't contain a uniform buffer meta block info with the name \"%s\"!", shader.GetName().GetData(), sBufferName.GetData());
      return EZ_FAILURE;
    }

    for (auto it = uniformBufferInfo->Variables.GetIterator(); it.IsValid(); ++it)
      m_Variables.Insert(it.Key(), Variable(it.Value(), this));

    return Init(uniformBufferInfo->iBufferDataSizeByte, sBufferName);
  }

  ezResult UniformBuffer::Init(std::initializer_list<const gl::ShaderObject*> metaInfos, const ezString& sBufferName)
  {
    EZ_ASSERT(metaInfos.size() != 0, "Meta info lookup list is empty!");

    bool initialized = false;
    int i = 0;
    for(auto shaderObjectIt = metaInfos.begin(); shaderObjectIt != metaInfos.end(); ++shaderObjectIt, ++i)
    {
      if(*shaderObjectIt == NULL) // the first was fatal, this one is skippable
      {
        ezLog::Warning("ShaderObject \"%s\" in list for uniform buffer \"%s\" initialization doesn't contain the needed meta data! Skipping..", 
                        (*shaderObjectIt)->GetName().GetData(), sBufferName.GetData());
        continue;
      }
      UniformBufferMetaInfo* uniformBufferInfo = nullptr;
      if (!(*shaderObjectIt)->GetUniformBufferInfo().TryGetValue(sBufferName, uniformBufferInfo)) // the first was fatal, this one is skippable
      {
        ezLog::Warning("ShaderObject \"%s\" in list for uniform buffer \"%s\" initialization doesn't contain the needed meta data! Skipping..", 
                        (*shaderObjectIt)->GetName().GetData(), sBufferName.GetData());
        continue;
      }

      if(!initialized)
      {
        ezResult result = Init(**metaInfos.begin(), sBufferName);
        if(result == EZ_SUCCESS)
          initialized = true;
        continue;
      }

      // sanity check
      if (uniformBufferInfo->iBufferDataSizeByte != m_uiBufferSizeBytes)
      {
        ezLog::Warning("ShaderObject \"%s\" in list for uniform buffer \"%s\" initialization gives size %i, first shader gave size %i! Skipping..",
          (*shaderObjectIt)->GetName().GetData(), sBufferName.GetData(), uniformBufferInfo->iBufferDataSizeByte, m_uiBufferSizeBytes);
        continue;
      }

      for (auto varIt = uniformBufferInfo->Variables.GetIterator(); varIt.IsValid(); ++varIt)
      {
        gl::UniformBuffer::Variable* ownVar = nullptr;
        if (m_Variables.TryGetValue(varIt.Key(), ownVar))  // overlap
        {
          // sanity check
          const gl::UniformVariableInfo* ownVarInfo =  &ownVar->GetMetaInfo();
          const gl::UniformVariableInfo* otherVarInfo = &varIt.Value();
          if (ezMemoryUtils::ByteCompare(ownVarInfo, otherVarInfo) != 0)
          {
            ezLog::Error("ShaderObject \"%s\" in list for uniform buffer \"%s\" initialization has a description of variable \"%s\" that doesn't match with the ones before!", 
                    (*shaderObjectIt)->GetName().GetData(), sBufferName.GetData(), varIt.Key().GetData());
          }
        }
        else // new one
        {
          m_Variables.Insert(varIt.Key(), Variable(varIt.Value(), this));
          // todo? check overlaps
        }
      }
    }

    return EZ_SUCCESS;
  }

  ezResult UniformBuffer::UpdateGPUData()
  {
    if(m_uiBufferDirtyRangeEnd <= m_uiBufferDirtyRangeStart)
      return EZ_SUCCESS;

    glBindBuffer(GL_UNIFORM_BUFFER, m_BufferObject);
    glBufferSubData(GL_UNIFORM_BUFFER, m_uiBufferDirtyRangeStart, m_uiBufferDirtyRangeEnd - m_uiBufferDirtyRangeStart, m_pBufferData + m_uiBufferDirtyRangeStart);

    ezResult result = gl::Utils::CheckError("glBufferSubData");

    m_uiBufferDirtyRangeEnd = std::numeric_limits<ezUInt32>::min();
    m_uiBufferDirtyRangeStart = std::numeric_limits<ezUInt32>::max();

    return result;
  }

  ezResult UniformBuffer::BindBuffer(GLuint locationIndex)
  {
    EZ_ASSERT(locationIndex < sizeof(s_pBoundUBOs) / sizeof(UniformBuffer*), "Can't bind ubo to slot %i. Maximum number of slots is %i", locationIndex, sizeof(s_pBoundUBOs) / sizeof(UniformBuffer*));

    if(UpdateGPUData() == EZ_FAILURE)
      return EZ_FAILURE;

    if(s_pBoundUBOs[locationIndex] != this)
    {
      glBindBufferBase(GL_UNIFORM_BUFFER, locationIndex, m_BufferObject);
      s_pBoundUBOs[locationIndex] = this;
      return gl::Utils::CheckError("glBindBufferBase");
    }
    
    return EZ_SUCCESS;
  }

}