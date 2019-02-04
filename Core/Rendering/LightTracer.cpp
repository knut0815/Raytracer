#include "PCH.h"
#include "LightTracer.h"
#include "Film.h"
#include "Context.h"
#include "Scene/Scene.h"
#include "Scene/Camera.h"
#include "Scene/Light/Light.h"
#include "Material/Material.h"
#include "Traversal/TraversalContext.h"

namespace rt {

using namespace math;

LightTracer::LightTracer(const Scene& scene)
    : IRenderer(scene)
{
}

const char* LightTracer::GetName() const
{
    return "Light Tracer";
}

const RayColor LightTracer::TraceRay_Single(const Ray&, const Camera& camera, Film& film, RenderingContext& ctx) const
{
    Uint32 depth = 0;

    const auto& allLocalLights = mScene.GetLights();
    if (allLocalLights.empty())
    {
        // no lights on the scene
        return RayColor::Zero();
    }

    const float lightPickingProbability = 1.0f / (float)allLocalLights.size();
    const Uint32 lightIndex = ctx.randomGenerator.GetInt() % (Uint32)allLocalLights.size();
    const LightPtr& light = allLocalLights[lightIndex];

    ILight::EmitResult emitResult;
    RayColor throughput = light->Emit(ctx, emitResult);

    if (throughput.AlmostZero())
    {
        // generated too weak sample - skip it
        return RayColor::Zero();
    }

    emitResult.emissionPdfW *= lightPickingProbability;
    RT_ASSERT(emitResult.emissionPdfW > 0.0f);

    Ray ray = Ray(emitResult.position, emitResult.direction);

    // TODO don't divide by pdf in ILight::Emit()
    throughput *= 1.0f / emitResult.emissionPdfW;

    for (;;)
    {
        HitPoint hitPoint;
        ctx.localCounters.Reset();
        mScene.Traverse_Single({ ray, hitPoint, ctx });
        ctx.counters.Append(ctx.localCounters);

        if (hitPoint.distance == FLT_MAX)
        {
            break; // ray missed
        }

        if (hitPoint.subObjectId == RT_LIGHT_OBJECT)
        {
            break; // we hit a light directly
        }

        // fill up structure with shading data
        ShadingData shadingData;
        {
            mScene.ExtractShadingData(ray, hitPoint, ctx.time, shadingData);

            shadingData.outgoingDirWorldSpace = -ray.dir;
            shadingData.outgoingDirLocalSpace = shadingData.WorldToLocal(shadingData.outgoingDirWorldSpace);

            RT_ASSERT(shadingData.material != nullptr);
            shadingData.material->EvaluateShadingData(ctx.wavelength, shadingData);
        }

        // check if the ray depth won't be exeeded in the next iteration
        if (depth >= ctx.params->maxRayDepth)
        {
            break;
        }

//        // Russian roulette algorithm
//        if (depth >= ctx.params->minRussianRouletteDepth)
//        {
//            Float threshold = throughput.Max();
//#ifdef RT_ENABLE_SPECTRAL_RENDERING
//            if (ctx.wavelength.isSingle)
//            {
//                threshold *= 1.0f / static_cast<Float>(Wavelength::NumComponents);
//            }
//#endif
//            if (ctx.randomGenerator.GetFloat() > threshold)
//            {
//                break;
//            }
//            throughput *= 1.0f / threshold;
//        }

        // connect to camera
        {
            const Vector4 cameraPos = camera.GetTransform().GetTranslation();
            const Vector4 samplePos = shadingData.frame.GetTranslation();

            Vector4 dirToCamera = cameraPos - samplePos;

            const Float cameraDistanceSqr = dirToCamera.SqrLength3();
            const Float cameraDistance = sqrtf(cameraDistanceSqr);

            dirToCamera /= cameraDistance;

            // calculate BSDF contribution
            float bsdfPdfW;
            const RayColor cameraFactor = shadingData.material->Evaluate(ctx.wavelength, shadingData, -dirToCamera, &bsdfPdfW);
            RT_ASSERT(cameraFactor.IsValid());

            if (!cameraFactor.AlmostZero())
            {
                Vector4 filmPos;

                if (camera.WorldToFilm(samplePos, filmPos))
                {
                    HitPoint shadowHitPoint;
                    shadowHitPoint.distance = cameraDistance * 0.999f;

                    const Ray shadowRay(samplePos + shadingData.frame[2] * 0.0001f, dirToCamera);

                    if (!mScene.Traverse_Shadow_Single({ shadowRay, shadowHitPoint, ctx }))
                    {
                        const Float cameraPdfA = camera.PdfW(-dirToCamera) / cameraDistanceSqr;
                        const RayColor contribution = (cameraFactor * throughput) * cameraPdfA;
                        const Vector4 value = contribution.ConvertToTristimulus(ctx.wavelength);
                        film.AccumulateColor(filmPos, value);
                    }
                }
            }
        }

        // sample BSDF
        Vector4 incomingDirWorldSpace;
        const RayColor bsdfValue = shadingData.material->Sample(ctx.wavelength, incomingDirWorldSpace, shadingData, ctx.randomGenerator);

        RT_ASSERT(bsdfValue.IsValid());
        throughput *= bsdfValue;

        // ray is not visible anymore
        if (throughput.AlmostZero())
        {
            break;
        }

        // generate secondary ray
        ray = Ray(shadingData.frame.GetTranslation(), incomingDirWorldSpace);
        ray.origin += ray.dir * 0.001f;

        depth++;
    }

    return RayColor::Zero();
}

} // namespace rt
