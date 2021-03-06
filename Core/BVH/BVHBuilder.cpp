#include "PCH.h"
#include "BVHBuilder.h"
#include "Utils/Logger.h"
#include "Utils/Timer.h"


namespace rt {

using namespace math;

BVHBuilder::Context::Context(uint32 numLeaves)
{
    mLeftBoxesCache.Resize_SkipConstructor(numLeaves);
    mRightBoxesCache.Resize_SkipConstructor(numLeaves);

    for (uint32 i = 0; i < NumAxes; ++i)
    {
        mSortedLeavesIndicesCache[i].Resize_SkipConstructor(numLeaves);
    }
}

//////////////////////////////////////////////////////////////////////////

BVHBuilder::BVHBuilder(BVH& targetBVH)
    : mLeafBoxes(nullptr)
    , mNumLeaves(0)
    , mNumGeneratedNodes(0)
    , mTarget(targetBVH)
{
}

BVHBuilder::~BVHBuilder()
{

}

bool BVHBuilder::Build(const Box* data, const uint32 numLeaves,
                       const BvhBuildingParams& params,
                       DynArray<uint32>& outLeavesOrder)
{
    mLeafBoxes = data;
    mNumLeaves = numLeaves;
    mParams = params;
    mTarget.AllocateNodes(2 * mNumLeaves); // TODO this is too big, reallocate at the end

    mNumGeneratedNodes = 0;
    mNumGeneratedLeaves = 0;
    mLeavesOrder.Clear();
    mLeavesOrder.Reserve(mNumLeaves);

    if (mNumLeaves == 0)
    {
        RT_LOG_INFO("Skipped empty BVH generation");
        return true;
    }

    // calculate overall bounding box
    Box overallBox = Box::Empty();
    for (uint32 i = 0; i < mNumLeaves; ++i)
    {
        overallBox = Box(overallBox, mLeafBoxes[i]);
    }

    RT_LOG_INFO("BVH statistics: num leaves = %u, overall box = [%f, %f, %f], [%f, %f, %f]",
                mNumLeaves,
                overallBox.min.f[0], overallBox.min.f[1], overallBox.min.f[2],
                overallBox.max.f[0], overallBox.max.f[1], overallBox.max.f[2]);

    WorkSet rootWorkSet;
    rootWorkSet.box = overallBox;
    rootWorkSet.numLeaves = mNumLeaves;
    rootWorkSet.leafIndices.Reserve(mNumLeaves);
    for (uint32 i = 0; i < mNumLeaves; ++i)
    {
        rootWorkSet.leafIndices.PushBack(i);
    }

    Timer timer;
    timer.Start();

    {
        Context context(mNumLeaves);

        BVH::Node& rootNode = mTarget.mNodes.Front();
        mNumGeneratedNodes += 2;
        BuildNode(rootWorkSet, context, rootNode);
    }

    RT_ASSERT(mNumGeneratedLeaves == mNumLeaves); // Number of generated leaves is invalid
    RT_ASSERT(mNumGeneratedNodes <= 2 * mNumLeaves); // Number of generated nodes is invalid

    // shrink BVH nodes array
    mTarget.mNumNodes = mNumGeneratedNodes;
    mTarget.mNodes.Resize(mNumGeneratedNodes);
    // mTarget.mNodes.shrink_to_fit(); // TODO

    const float millisecondsElapsed = (float)(1000.0 * timer.Stop());
    RT_LOG_INFO("Finished BVH generation in %.9g ms (num nodes = %u)", millisecondsElapsed, mNumGeneratedNodes);

    outLeavesOrder = mLeavesOrder;
    return true;
}

void BVHBuilder::GenerateLeaf(const WorkSet& workSet, BVH::Node& targetNode)
{
    targetNode.numLeaves = workSet.numLeaves;
    targetNode.childIndex = mNumGeneratedLeaves;

    for (uint32 i = 0; i < workSet.numLeaves; ++i)
    {
        mLeavesOrder.PushBack(workSet.leafIndices[i]);
    }

    mNumGeneratedLeaves += workSet.numLeaves;
}

void BVHBuilder::BuildNode(const WorkSet& workSet, Context& context, BVH::Node& targetNode)
{
    RT_ASSERT(workSet.numLeaves <= mNumLeaves);
    RT_ASSERT(workSet.numLeaves > 0);
    RT_ASSERT(workSet.depth < mNumLeaves);
    RT_ASSERT(workSet.depth <= BVH::MaxDepth);

    targetNode.min = workSet.box.min.ToFloat3();
    targetNode.max = workSet.box.max.ToFloat3();

    if (workSet.numLeaves <= mParams.maxLeafNodeSize)
    {
        GenerateLeaf(workSet, targetNode);
        return;
    }

    uint32 bestAxis = 0;
    uint32 bestSplitPos = 0;
    float bestCost = FLT_MAX;
    Box bestLeftBox = Box::Empty();
    Box bestRightBox = Box::Empty();

    SortLeaves(workSet, context);

    for (uint32 axis = 0; axis < NumAxes; ++axis)
    {
        const Indices& sortedIndices = context.mSortedLeavesIndicesCache[axis];

        // calculate left child node AABB for each possible split position
        {
            Box accumulatedBox = Box::Empty();
            for (uint32 i = 0; i < workSet.numLeaves; ++i)
            {
                accumulatedBox = Box(accumulatedBox, mLeafBoxes[sortedIndices[i]]);
                context.mLeftBoxesCache[i] = accumulatedBox;
            }
        }

        // calculate right child node AABB for each possible split position
        {
            Box accumulatedBox = Box::Empty();
            for (uint32 i = workSet.numLeaves; i-- > 0; )
            {
                accumulatedBox = Box(accumulatedBox, mLeafBoxes[sortedIndices[i]]);
                context.mRightBoxesCache[i] = accumulatedBox;
            }
        }

        // find optimal split position (surface area heuristics)
        for (uint32 splitPos = 0; splitPos < workSet.numLeaves - 1; ++splitPos)
        {
            const Box& leftBox = context.mLeftBoxesCache[splitPos];
            const Box& rightBox = context.mRightBoxesCache[splitPos + 1];

            float leftCost = 0.0f, rightCost = 0.0f;
            if (mParams.heuristics == BvhBuildingParams::Heuristics::SurfaceArea)
            {
                leftCost = leftBox.SurfaceArea();
                rightCost = rightBox.SurfaceArea();
            }
            else if (mParams.heuristics == BvhBuildingParams::Heuristics::Volume)
            {
                leftCost = leftBox.Volume();
                rightCost = rightBox.Volume();
            }
            else
            {
                RT_FATAL();
            }

            const uint32 leftCount = splitPos + 1;
            const uint32 rightCount = workSet.numLeaves - leftCount;
            const float totalCost = leftCost * static_cast<float>(leftCount) + rightCost * static_cast<float>(rightCount);

            if (totalCost < bestCost)
            {
                bestCost = totalCost;
                bestAxis = axis;
                bestSplitPos = splitPos;
                bestLeftBox = leftBox;
                bestRightBox = rightBox;
            }
        }
    }

    const uint32 leftCount = bestSplitPos + 1;
    const uint32 rightCount = workSet.numLeaves - leftCount;

    const uint32 leftNodeIndex = mNumGeneratedNodes;
    mNumGeneratedNodes += 2;

    targetNode.childIndex = leftNodeIndex;
    targetNode.numLeaves = 0;
    targetNode.splitAxis = bestAxis;

    WorkSet childWorkSet;
    childWorkSet.sortedBy = bestAxis;
    childWorkSet.depth = workSet.depth + 1;

    const Indices& sortedIndices = context.mSortedLeavesIndicesCache[bestAxis];

    Indices leftIndices, rightIndices;
    leftIndices.Resize_SkipConstructor(leftCount);
    rightIndices.Resize_SkipConstructor(rightCount);
    memcpy(leftIndices.Data(), sortedIndices.Data(), sizeof(uint32) * leftCount);
    memcpy(rightIndices.Data(), sortedIndices.Data() + leftCount, sizeof(uint32) * rightCount);

    // generate left node
    {
        BVH::Node& childNode = mTarget.mNodes[leftNodeIndex];

        childWorkSet.box = bestLeftBox;
        childWorkSet.numLeaves = leftCount;
        childWorkSet.leafIndices = std::move(leftIndices);
        BuildNode(childWorkSet, context, childNode);
    }

    // generate right node
    {
        BVH::Node& childNode = mTarget.mNodes[leftNodeIndex + 1];

        childWorkSet.box = bestRightBox;
        childWorkSet.numLeaves = rightCount;
        childWorkSet.leafIndices = std::move(rightIndices);
        BuildNode(childWorkSet, context, childNode);
    }
}

void BVHBuilder::SortLeaves(const WorkSet& workSet, Context& context) const
{
    for (uint32 axis = 0; axis < NumAxes; ++axis)
    {
        Indices& indicesToSort = context.mSortedLeavesIndicesCache[axis];

        if (workSet.sortedBy != axis) // sort only what needs to be sorted
        {
            indicesToSort = workSet.leafIndices;

            const auto comparator = [this, axis](const uint32 a, const uint32 b)
            {
                RT_ASSERT(a < mNumLeaves);
                RT_ASSERT(b < mNumLeaves);

                const Box& leafA = mLeafBoxes[a];
                const Box& leafB = mLeafBoxes[b];

                // TODO use precalculated triangle center (experiment)
                const Vector4 centerA = leafA.max + leafA.min;
                const Vector4 centerB = leafB.max + leafB.min;
                return centerA[axis] < centerB[axis];
            };

            std::sort(indicesToSort.begin(), indicesToSort.end(), comparator);
        }
    }

    if (workSet.sortedBy < NumAxes)
    {
        context.mSortedLeavesIndicesCache[workSet.sortedBy] = std::move(workSet.leafIndices);
    }
}

} // namespace rt
