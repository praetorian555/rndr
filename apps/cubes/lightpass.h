//#pragma once
//
//#include "rndr/rndr.h"
//
//class LightRenderPass
//{
//public:
//    void Init(rndr::GraphicsContext* GraphicsContext, rndr::ProjectionCamera* Camera);
//    void ShutDown();
//
//    void Render(real DeltaSeconds);
//
//    rndr::Point3r GetLightPosition() const;
//
//private:
//#if defined RNDR_RASTER
//    void VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo);
//    void FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo);
//#endif  // RNDR_RASTER
//
//private:
//    rndr::GraphicsContext* m_GraphicsContext;
//    std::unique_ptr<rndr::Pipeline> m_Pipeline;
//    std::unique_ptr<rndr::Model> m_Model;
//
//    rndr::ProjectionCamera* m_Camera = nullptr;
//
//    rndr::Point3r m_LightPosition;
//};