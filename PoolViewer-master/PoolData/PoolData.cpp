#include <windows.h>
#include <winternl.h>
#include <list>
#include <iterator>
#include <vector>
#include <algorithm>
#include "PoolData.h"

using namespace std;

PDEBUG_CLIENT g_DebugClient = nullptr;
PDEBUG_SYMBOLS g_DebugSymbols = nullptr;
PDEBUG_DATA_SPACES4 g_DataSpaces = nullptr;
PDEBUG_CONTROL g_DebugControl = nullptr;

#define ALIGN_DOWN_POINTER_BY(address, alignment) \
    ((PVOID)((ULONG_PTR)(address) & ~((ULONG_PTR)(alignment) - 1)))

#define ALIGN_UP_POINTER_BY(address, alignment) \
    (ALIGN_DOWN_POINTER_BY(((ULONG_PTR)(address) + (alignment) - 1), alignment))

#define CONTROL_KERNEL_DUMP 37
static NtSystemDebugControl g_NtSystemDebugControl = NULL;

ULONG64 g_HeapKey;
ULONG64 g_LfhKey;
ULONG64 g_KernBase;
list<HEAP> g_Heaps;
list<ALLOC> g_CurrentAllocs;

ULONG64 g_PoolState;
ULONG64 g_PoolBigPageTableOffset;
ULONG64 g_PoolBigPageTable;
ULONG64 g_PoolBigPageTableSizeOffset;
ULONG64 g_PoolBigPageTableSize;
HEAP_MANAGER_STATE_OFFSETS g_HeapManagerStateOffsets;
RTLP_HP_HEAP_MANAGER_OFFSETS g_HpHeapManagerOffsets;
RTLP_HP_ALLOC_TRACKER_OFFSETS g_RtlpHpAllocTrackerOffsets;
RTL_CSPARSE_BITMAP_OFFSETS g_RtlCsparseBitmapOffsets;
HEAP_VAMGR_CTX_OFFSETS g_VamgrCtxOffsets;
HEAP_VAMGR_VASPACE_OFFSETS g_VamgrVaspaceOffsets;
RTL_SPARSE_ARRAY_OFFSETS g_RtlSparseArrayOffsets;
HEAP_VAMGR_RANGE_OFFSETS g_VamgrRangeOffsets;
HEAP_SEG_CONTEXT_OFFSETS g_SegContextOffsets;
HEAP_VS_CONTEXT_OFFSETS g_HeapVsContext;
HEAP_LFH_CONTEXT_OFFSETS g_HeapLfhContext;
HEAP_PAGE_SEGMENT_OFFSETS g_PageSegmentOffsets;
HEAP_PAGE_RANGE_DESCRIPTOR_OFFSET g_PageRangeDescriptorOffsets;
SEGMENT_HEAP_OFFSETS g_SegmentHeapOffsets;
EX_HEAP_POOL_NODE_OFFSETS g_PoolNodeOffsets;
HEAP_VS_SUBSEGMENT_OFFSETS g_VsSubsegmentOffsets;
HEAP_VS_CHUNK_HEADER_OFFSETS g_VsChunkHeaderOffsets;
HEAP_VS_CHUNK_HEADER_SIZE_OFFSETS g_VsChunkHeaderSizeOffsets;
POOL_HEADER_OFFSETS g_PoolHeaderOffsets;
HEAP_LFH_SUBSEGMENT_OFFSETS g_lfhSubsegmentOffsets;
HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS_OFFSETS g_lfhSubsegmentEncodedEffsets;
POOL_TRACKER_BIG_PAGES_OFFSETS g_PoolTrackerBigPagesOffsets;
HEAP_LARGE_ALLOC_DATA_OFFSETS g_HeapLargeAllocDataOffsets;
RTL_BALANCED_NODE_OFFSETS g_RtlBalancedNodeOffsets;
RTL_RB_TREE_OFFSETS g_RtlRbTreeOffsets;

//
// Types
//
ULONG RTLP_HP_HEAP_GLOBALS;
ULONG EX_POOL_HEAP_MANAGER_STATE;
ULONG RTLP_HP_HEAP_MANAGER;
ULONG RTLP_HP_ALLOC_TRACKER;
ULONG RTL_CSPARSE_BITMAP;
ULONG EX_HEAP_POOL_NODE;
ULONG SEGMENT_HEAP;
ULONG HEAP_SEG_CONTEXT;
ULONG HEAP_PAGE_SEGMENT;
ULONG HEAP_PAGE_RANGE_DESCRIPTOR;
ULONG HEAP_VAMGR_CTX;
ULONG HEAP_VAMGR_VASPACE;
ULONG RTL_SPARSE_ARRAY;
ULONG HEAP_VAMGR_RANGE;
ULONG HEAP_VS_SUBSEGMENT;
ULONG HEAP_VS_CHUNK_HEADER;
ULONG HEAP_VS_CHUNK_HEADER_SIZE;
ULONG POOL_HEADER;
ULONG HEAP_LFH_SUBSEGMENT;
ULONG HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS;
ULONG POOL_TRACKER_BIG_PAGES;
ULONG HEAP_LARGE_ALLOC_DATA;
ULONG RTL_BALANCED_NODE;
ULONG RTL_RB_TREE;
ULONG HEAP_VS_CONTEXT;
ULONG HEAP_LFH_CONTEXT;
ULONG HEAP_LFH_BUCKET;

//
// Sizes
//
ULONG g_HeapPageSegSize;
ULONG g_RangeDescriptorSize;
ULONG g_VamgrRangeSize;
ULONG g_SegContextSize;
ULONG g_PoolNodeSize;
ULONG g_vsSubsegmentSize;
ULONG g_vsChunkHeaderSize;
ULONG g_poolHeaderSize;
ULONG g_poolTrackerBigPagesSize;
ULONG g_HeapLargeAllocDataSize;
ULONG g_RtlRbTreeSize;

/*
	Initializes all types required by this tool as globals so we don't have to
	find the necessary types in each function.
*/
HRESULT
GetTypes(
	void
)
{
	if ((!SUCCEEDED(g_DataSpaces->ReadDebuggerData(DEBUG_DATA_KernBase, &g_KernBase, sizeof(g_KernBase), nullptr))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTLP_HP_HEAP_GLOBALS", &RTLP_HP_HEAP_GLOBALS))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_EX_POOL_HEAP_MANAGER_STATE", &EX_POOL_HEAP_MANAGER_STATE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTLP_HP_HEAP_MANAGER", &RTLP_HP_HEAP_MANAGER))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTLP_HP_ALLOC_TRACKER", &RTLP_HP_ALLOC_TRACKER))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTL_CSPARSE_BITMAP", &RTL_CSPARSE_BITMAP))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_EX_HEAP_POOL_NODE", &EX_HEAP_POOL_NODE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_SEGMENT_HEAP", &SEGMENT_HEAP))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_SEG_CONTEXT", &HEAP_SEG_CONTEXT))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_PAGE_SEGMENT", &HEAP_PAGE_SEGMENT))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_PAGE_RANGE_DESCRIPTOR", &HEAP_PAGE_RANGE_DESCRIPTOR))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VAMGR_CTX", &HEAP_VAMGR_CTX))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VAMGR_VASPACE", &HEAP_VAMGR_VASPACE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTL_SPARSE_ARRAY", &RTL_SPARSE_ARRAY))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VAMGR_RANGE", &HEAP_VAMGR_RANGE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VS_SUBSEGMENT", &HEAP_VS_SUBSEGMENT))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VS_CHUNK_HEADER", &HEAP_VS_CHUNK_HEADER))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VS_CHUNK_HEADER_SIZE", &HEAP_VS_CHUNK_HEADER_SIZE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_POOL_HEADER", &POOL_HEADER))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_LFH_SUBSEGMENT", &HEAP_LFH_SUBSEGMENT))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS", &HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_POOL_TRACKER_BIG_PAGES", &POOL_TRACKER_BIG_PAGES))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_LARGE_ALLOC_DATA", &HEAP_LARGE_ALLOC_DATA))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTL_BALANCED_NODE", &RTL_BALANCED_NODE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_RTL_RB_TREE", &RTL_RB_TREE))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_VS_CONTEXT", &HEAP_VS_CONTEXT)))||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_LFH_CONTEXT", &HEAP_LFH_CONTEXT)))||
		(!SUCCEEDED(g_DebugSymbols->GetTypeId(g_KernBase, "_HEAP_LFH_BUCKET", &HEAP_LFH_BUCKET))))
	{
		return S_FALSE;
	}

	return S_OK;
}

/*
	Initializes all the sizes of structures that this tool uses as globals.
*/
HRESULT
GetSizes(
	void
)
{
	if ((!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_PAGE_SEGMENT, &g_HeapPageSegSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_PAGE_RANGE_DESCRIPTOR, &g_RangeDescriptorSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_VAMGR_RANGE, &g_VamgrRangeSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_SEG_CONTEXT, &g_SegContextSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, EX_HEAP_POOL_NODE, &g_PoolNodeSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_VS_SUBSEGMENT, &g_vsSubsegmentSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_VS_CHUNK_HEADER, &g_vsChunkHeaderSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, POOL_HEADER, &g_poolHeaderSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, POOL_TRACKER_BIG_PAGES, &g_poolTrackerBigPagesSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, RTL_RB_TREE, &g_RtlRbTreeSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetTypeSize(g_KernBase, HEAP_LARGE_ALLOC_DATA, &g_HeapLargeAllocDataSize)))
		)
	{
		printf("GetTypeSize failed\n");
		return S_FALSE;
	}
	return S_OK;
}

/*
	Initialize HeapKey and LfhKey.
*/
HRESULT
GetHeapGlobals(
	void
)
{
	HRESULT result;
	ULONG64 heapGlobals;
	ULONG lfhKeyOffset;
	ULONG64 lfhKeyAddress;
	ULONG64 lfhKey;
	ULONG heapKeyOffset;
	ULONG64 heapKeyAddress;
	ULONG64 heapKey;

	if ((!SUCCEEDED(g_DebugSymbols->GetOffsetByName("nt!RtlpHpHeapGlobals", &heapGlobals))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_HEAP_GLOBALS, "LfhKey", &lfhKeyOffset))))
	{
		result = S_FALSE;
		goto Exit;
	}

	lfhKeyAddress = heapGlobals + lfhKeyOffset;

	result = g_DataSpaces->ReadVirtual(lfhKeyAddress, &lfhKey, sizeof(lfhKey), nullptr);
	if (!SUCCEEDED(result))
	{
		goto Exit;
	}
	g_LfhKey = lfhKey;

	result = g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_HEAP_GLOBALS, "HeapKey", &heapKeyOffset);
	if (!SUCCEEDED(result))
	{
		goto Exit;
	}

	heapKeyAddress = heapGlobals + heapKeyOffset;

	result = g_DataSpaces->ReadVirtual(heapKeyAddress, &heapKey, sizeof(heapKey), nullptr);
	if (!SUCCEEDED(result))
	{
		goto Exit;
	}
	g_HeapKey = heapKey;

Exit:
	return result;
}

/*
	Get offsets for all the structures that this tool uses.
*/
HRESULT
GetOffsets(
	void
)
{
	if ((!SUCCEEDED(g_DebugSymbols->GetOffsetByName("nt!ExPoolState", &g_PoolState))) ||
		(!SUCCEEDED(g_DebugSymbols->GetOffsetByName("nt!PoolBigPageTable", &g_PoolBigPageTableOffset))) ||
		(!SUCCEEDED(g_DataSpaces->ReadVirtual(g_PoolBigPageTableOffset, &g_PoolBigPageTable, sizeof(g_PoolBigPageTable), nullptr))) ||
		(!SUCCEEDED(g_DebugSymbols->GetOffsetByName("nt!PoolBigPageTableSize", &g_PoolBigPageTableSizeOffset))) ||
		(!SUCCEEDED(g_DataSpaces->ReadVirtual(g_PoolBigPageTableSizeOffset, &g_PoolBigPageTableSize, sizeof(g_PoolBigPageTableSize), nullptr))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, EX_POOL_HEAP_MANAGER_STATE, "HeapManager", &g_HeapManagerStateOffsets.HeapManagerOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, EX_POOL_HEAP_MANAGER_STATE, "PoolNode", &g_HeapManagerStateOffsets.PoolNodeOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, EX_POOL_HEAP_MANAGER_STATE, "NumberOfPools", &g_HeapManagerStateOffsets.NumberOfPoolsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, EX_POOL_HEAP_MANAGER_STATE, "SpecialHeaps", &g_HeapManagerStateOffsets.SpecialHeapsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_HEAP_MANAGER, "AllocTracker", &g_HpHeapManagerOffsets.AllocTrackerOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_HEAP_MANAGER, "VaMgr", &g_HpHeapManagerOffsets.VaMgrOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_ALLOC_TRACKER, "AllocTrackerBitmap", &g_RtlpHpAllocTrackerOffsets.AllocTrackerBitmapOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTLP_HP_ALLOC_TRACKER, "BaseAddress", &g_RtlpHpAllocTrackerOffsets.BaseAddressOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_CSPARSE_BITMAP, "CommitDirectory", &g_RtlCsparseBitmapOffsets.CommitDirectoryOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_CSPARSE_BITMAP, "CommitBitmap", &g_RtlCsparseBitmapOffsets.CommitBitmapOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_CSPARSE_BITMAP, "UserBitmap", &g_RtlCsparseBitmapOffsets.UserBitmapOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VAMGR_CTX, "VaSpace", &g_VamgrCtxOffsets.VaSpaceOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VAMGR_VASPACE, "VaRangeArray", &g_VamgrVaspaceOffsets.VaRangeArrayOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VAMGR_VASPACE, "BaseAddress", &g_VamgrVaspaceOffsets.BaseAddressOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_SPARSE_ARRAY, "ElementSizeShift", &g_RtlSparseArrayOffsets.ElementSizeShiftOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_SPARSE_ARRAY, "Bitmap", &g_RtlSparseArrayOffsets.BitmapOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VAMGR_RANGE, "Allocated", &g_VamgrRangeOffsets.AllocatedOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "SegmentListHead", &g_SegContextOffsets.SegmentListHeadOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "FirstDescriptorIndex", &g_SegContextOffsets.FirstDescriptorIndexOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "SegmentCount", &g_SegContextOffsets.SegmentCountOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "UnitShift", &g_SegContextOffsets.UnitShiftOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "VsContext", &g_SegContextOffsets.VsContextOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_SEG_CONTEXT, "LfhContext", &g_SegContextOffsets.LfhContextOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_CONTEXT, "SubsegmentList", &g_HeapVsContext.SubsegmentListOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_CONTEXT, "Buckets", &g_HeapLfhContext.BucketsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_SEGMENT, "ListEntry", &g_PageSegmentOffsets.ListEntryOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_SEGMENT, "Signature", &g_PageSegmentOffsets.SignatureOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_SEGMENT, "DescArray", &g_PageSegmentOffsets.DescArrayOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_SEGMENT, "SegmentCommitState", &g_PageSegmentOffsets.SegmentCommitStateOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_RANGE_DESCRIPTOR, "RangeFlags", &g_PageRangeDescriptorOffsets.RangeFlagsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_RANGE_DESCRIPTOR, "TreeSignature", &g_PageRangeDescriptorOffsets.TreeSignatureOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_PAGE_RANGE_DESCRIPTOR, "UnitSize", &g_PageRangeDescriptorOffsets.UnitSizeOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, SEGMENT_HEAP, "SegContexts", &g_SegmentHeapOffsets.SegContextsOffsets))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, SEGMENT_HEAP, "LargeAllocMetadata", &g_SegmentHeapOffsets.LargeAllocMetadataOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, EX_HEAP_POOL_NODE, "Heaps", &g_PoolNodeOffsets.HeapsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_SUBSEGMENT, "Signature", &g_VsSubsegmentOffsets.SignatureOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_SUBSEGMENT, "Size", &g_VsSubsegmentOffsets.SizeOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_CHUNK_HEADER, "Sizes", &g_VsChunkHeaderOffsets.SizesOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_CHUNK_HEADER_SIZE, "HeaderBits", &g_VsChunkHeaderSizeOffsets.HeaderBitsOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_CHUNK_HEADER_SIZE, "UnsafeSize", &g_VsChunkHeaderSizeOffsets.UnsafeSizeOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_VS_CHUNK_HEADER_SIZE, "Allocated", &g_VsChunkHeaderSizeOffsets.AllocatedOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, POOL_HEADER, "PoolTag", &g_PoolHeaderOffsets.PoolTagOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_SUBSEGMENT, "BlockOffsets", &g_lfhSubsegmentOffsets.BlockOffsets))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_SUBSEGMENT, "BlockBitmap", &g_lfhSubsegmentOffsets.BlockBitmap))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS, "EncodedData", &g_lfhSubsegmentEncodedEffsets.EncodedData))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS, "BlockSize", &g_lfhSubsegmentEncodedEffsets.BlockSize))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS, "FirstBlockOffset", &g_lfhSubsegmentEncodedEffsets.FirstBlockOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, POOL_TRACKER_BIG_PAGES, "Va", &g_PoolTrackerBigPagesOffsets.Va))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, POOL_TRACKER_BIG_PAGES, "Key", &g_PoolTrackerBigPagesOffsets.Key))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, POOL_TRACKER_BIG_PAGES, "NumberOfBytes", &g_PoolTrackerBigPagesOffsets.NumberOfBytes))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LARGE_ALLOC_DATA, "TreeNode", &g_HeapLargeAllocDataOffsets.TreeNodeOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, HEAP_LARGE_ALLOC_DATA, "VirtualAddress", &g_HeapLargeAllocDataOffsets.VirtualAddressOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_RB_TREE, "Root", &g_RtlRbTreeOffsets.RootOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_RB_TREE, "Encoded", &g_RtlRbTreeOffsets.EncodedOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_BALANCED_NODE, "Left", &g_RtlBalancedNodeOffsets.LeftOffset))) ||
		(!SUCCEEDED(g_DebugSymbols->GetFieldOffset(g_KernBase, RTL_BALANCED_NODE, "Right", &g_RtlBalancedNodeOffsets.RightOffset))))
	{
		return S_FALSE;
	}

	return S_OK;
}

/*
	Validates the region that is about to be read and reads it.
*/
HRESULT
ReadData(
	_In_ ULONG64 Address,
	_In_ ULONG Size,
	_Out_ PVOID Buffer
)
{
	HRESULT result;
	ULONG64 validBase;
	ULONG validSize;

	result = g_DataSpaces->GetValidRegionVirtual(Address,
		Size,
		&validBase,
		&validSize);
	if (result != S_OK)
	{
		return result;
	}
	if (validBase != Address)
	{
		return S_FALSE;
	}
	return g_DataSpaces->ReadVirtual(Address,
		Buffer,
		Size,
		nullptr);
}

/*
	Check the bitmap value for an address.
*/
ULONG
BitmapBitmaskRead(
	_In_ PVOID Address
)
{
	RTLP_CSPARSE_BITMAP_STATE state;
	USHORT bitmapResult;
	HRESULT result;
	ULONG64 heapManagaerAddress;
	ULONG64 allocTrackerAddress;
	ULONG64 allocTrackerBitmapAddress;
	ULONG64 baseAddressAddress;
	ULONG64 baseAddress;
	ULONG bitIndex;
	CHAR SBitmap[256];
	ULONG64 commitBitmap;
	ULONG64* userBitmap;
	LONG64 commitBitmapValue;
	ULONG64 userBitmapValue;

	bitmapResult = 0;

	heapManagaerAddress = g_PoolState + g_HeapManagerStateOffsets.HeapManagerOffset;
	allocTrackerAddress = heapManagaerAddress + g_HpHeapManagerOffsets.AllocTrackerOffset;
	allocTrackerBitmapAddress = allocTrackerAddress + g_RtlpHpAllocTrackerOffsets.AllocTrackerBitmapOffset;
	baseAddressAddress = allocTrackerAddress + g_RtlpHpAllocTrackerOffsets.BaseAddressOffset;

	//
	// Get BaseAddress and calculate bitIndex
	//
	if ((!SUCCEEDED(g_DataSpaces->ReadVirtual(
		baseAddressAddress, &baseAddress, sizeof(baseAddress), nullptr))) ||
		(!SUCCEEDED(g_DebugSymbols->ReadTypedDataVirtual(
			allocTrackerBitmapAddress, g_KernBase, RTL_CSPARSE_BITMAP, (PVOID)SBitmap, sizeof(SBitmap), NULL))))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed reading bitmap base\n", __FUNCTIONW__, __LINE__);
		result = S_FALSE;
		goto Exit;
	}
	bitIndex = 2 * (((ULONG64)Address - baseAddress) >> 20);

	//
	// First check the CommitDirectory - this has a bit for each GB of user pages. 
	// The bit marks whether there are any pages in the GB that are heap pages. If there aren't, 
	// no memory is allocated in the UserBitmap for these pages.
	//

	if (_bittest64(
		(LONG64*)(&SBitmap[g_RtlCsparseBitmapOffsets.CommitDirectoryOffset]),
		bitIndex >> 30))
	{
		commitBitmap = *(ULONG64*)((ULONG64)SBitmap + g_RtlCsparseBitmapOffsets.CommitBitmapOffset);
		//
		// Divide by 8 to get the right byte to read
		//
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(
			commitBitmap + ((bitIndex >> 15) / 8),
			&commitBitmapValue,
			sizeof(commitBitmapValue),
			nullptr)))
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed reading commit bitmap\n", __FUNCTIONW__, __LINE__);
			result = S_FALSE;
			goto Exit;
		}
		//
		// Test bitIndex % 8 to know which bit we should read in the first byte
		//
		if (_bittest64(&commitBitmapValue, (bitIndex >> 15) % 8))
		{
			state = RTLP_CSPARSE_BITMAP_STATE::UserBitmapValid;
		}
		else
		{
			state = RTLP_CSPARSE_BITMAP_STATE::UserBitmapInvalid;
		}
	}
	else
	{
		state = RTLP_CSPARSE_BITMAP_STATE::CommitBitmapInvalid;
	}
	if (state == RTLP_CSPARSE_BITMAP_STATE::UserBitmapValid)
	{
		//
		// We need to read the address in SBitmap->UserBitmap and then we need
		// userBitmap to be a pointer so the pointer arythmetic on the next line works.
		// This comes out very ugly, there must be a nicer way of getting it to work.
		//
		userBitmap = (ULONG64*)*(ULONG64*)((ULONG64)SBitmap + g_RtlCsparseBitmapOffsets.UserBitmapOffset);
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(
			(ULONG64)(userBitmap + (bitIndex >> 6)),
			&userBitmapValue,
			sizeof(userBitmapValue),
			nullptr)))
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed reading user bitmap\n", __FUNCTIONW__, __LINE__);
			result = S_FALSE;
			goto Exit;
		}
		bitmapResult = (userBitmapValue >> (bitIndex & 0x3F)) & 3;
	}
	else
	{
		bitmapResult = 0;
	}

Exit:
	return bitmapResult;
}

/*
	Retrieves a pool tag for a big pool allocation from the big pages table.
*/
ULONG
GetTagFromBigPagesTable(
	_In_ ULONG64 Address,
	_Out_ std::string* Tag
)
{
	PVOID poolTrackerBigPages;
	ULONG64 hash;
	UCHAR* key;
	HRESULT result;
	ULONG64 va;
	ULONG64 numberOfBytes;
	std::string tag;

	poolTrackerBigPages = nullptr;
	tag = "";
	numberOfBytes = 0;

	poolTrackerBigPages = VirtualAlloc(NULL, g_poolTrackerBigPagesSize, MEM_COMMIT, PAGE_READWRITE);
	if (poolTrackerBigPages == nullptr)
	{
		goto Exit;
	}
	//
	// Find the tag in PoolBigPageTable.
	//

	//
	// Calculate the hash to find the allocation in the table.
	//
	hash = ((ULONG64)((ULONG)(Address >> 0xc))) * 0x9E5F;
	hash = hash ^ (hash >> 0x20);
	result = g_DataSpaces->ReadVirtual(
		g_PoolBigPageTable + ((hash & (g_PoolBigPageTableSize - 1)) * g_poolTrackerBigPagesSize),
		poolTrackerBigPages,
		g_poolTrackerBigPagesSize,
		nullptr);
	if (SUCCEEDED(result))
	{
		//
		// Build the tag string and get the size of the allocation.
		//
		va = *(ULONG64*)(((ULONG64)poolTrackerBigPages) + g_PoolTrackerBigPagesOffsets.Va);
		if (va != Address)
		{
			//
			// This means that we found the wrong entry.
			// There is a bug that I couldn't figure out yet where
			// sometimes we get an off-by-one error and the correct entry
			// is the next one. So try the next entry and if that one is wrong
			// too, just bail and return empty values for this allocation.
			//
			hash += 1;
			result = g_DataSpaces->ReadVirtual(
				g_PoolBigPageTable + ((hash & (g_PoolBigPageTableSize - 1)) * g_poolTrackerBigPagesSize),
				poolTrackerBigPages,
				g_poolTrackerBigPagesSize,
				nullptr);
			if (SUCCEEDED(result))
			{
				va = *(ULONG64*)(((ULONG64)poolTrackerBigPages) + g_PoolTrackerBigPagesOffsets.Va);
				if (va != Address)
				{
					goto Exit;
				}
			}
		}
		key = (UCHAR*)(((ULONG64)poolTrackerBigPages) + g_PoolTrackerBigPagesOffsets.Key);
		numberOfBytes = *(ULONG*)(((ULONG64)poolTrackerBigPages) + g_PoolTrackerBigPagesOffsets.NumberOfBytes);

		tag += *key;
		tag += *(key + 1);
		tag += *(key + 2);
		tag += *(key + 3);
		tag += '\x0';
	}
Exit:
	*Tag = tag;
	return numberOfBytes;
}

/*
	Prints information about a pool block to the debugger console.

	@param[in] Address - Base address of the block
	@param[in] Size - Size of the block
	@param[in] Allocated - Is the block allocated or free
	@param[in] Type - Subsegment type (Vs, Lfh, Big, Large)
	@param[in,opt] PoolHeaderAddress - Address to search for the pool header.
		Only needed for blocks smaller than 0xFF8.
		Needs to be sent by the caller because it can slightly change based on the
		type and alignment of the pool block.
	@param[in] Highlight - If set to TRUE function will mark the allocation with *
	@param[in,opt] - Optional pool tag. Block will only be printed if it has a matching pool tag.
*/
HRESULT PrintInfoForSingleBlock(
	_In_ ULONG64 Address,
	_In_ ULONG Size,
	_In_ BOOLEAN Allocated,
	_In_ ALLOCATION_TYPE Type,
	_In_opt_ ULONG64 PoolHeaderAddress,
	_In_ BOOLEAN Highlight,
	_In_opt_ PCSTR Tag,
	_Out_opt_ std::list<ALLOC>* Allocations
)
{
	HRESULT result;
	ALLOC allocation;
	ULONG numberOfBytes;
	UCHAR* poolTag;
	ULONG64 poolHeaderAddress;
	PVOID poolHeader = nullptr;
	std::string spaces;
	std::string type;
	result = S_OK;
	if(Size==0)
	{
		return S_OK;
	}
	if (Tag && !Allocated)
	{
		//
		// If a block isn't allocated we don't want to include it in our tag search
		//
		return S_OK;
	}

	poolHeader = VirtualAlloc(NULL, g_poolHeaderSize, MEM_COMMIT, PAGE_READWRITE);
	if (poolHeader == nullptr)
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed to allocate pool header\n", __FUNCTIONW__, __LINE__);
		result = S_FALSE;
		goto Exit;
	}
	//
	// Collect information for the block.
	//
	allocation.Address = Address;
	allocation.Size = Size;
	allocation.Allocated = Allocated;
	allocation.PoolTag = "";
	allocation.Type = Type;

	allocation.PoolTag = "    ";
	allocation.PoolTag += '\x0';

	if (Size >= 0xFF8)
	{
		allocation.PoolTag = "";
		numberOfBytes = GetTagFromBigPagesTable(allocation.Address, &allocation.PoolTag);
	}
	//
	// Get pool header
	//
	else if (PoolHeaderAddress != 0)
	{
		//
		// The pool header is never on the last 0x10 bytes of a page.
		// If the chunk header ends at 0x...ff0, the pool header will be in the beginning
		// of the next page.
		//
		poolHeaderAddress = PoolHeaderAddress;
		if (0x1000 - (poolHeaderAddress & 0xfff) == 0x10)
		{
			poolHeaderAddress += 0x10;
			allocation.Size -= 0x10;
			allocation.Address += 0x10;
		}
		if (SUCCEEDED(g_DataSpaces->ReadVirtual(poolHeaderAddress, poolHeader, g_poolHeaderSize, nullptr)))
		{
			poolTag = (UCHAR*)((ULONG64)poolHeader + g_PoolHeaderOffsets.PoolTagOffset);
			//
			// Get the Tag from the pool header and build the tag string.
			//If pool tag has 0xffff..., this has a pointer instead of a tag, ignore.
			//
			if ((*(poolTag + 2) != 0xff) && (*(poolTag + 3) != 0xff) && (*(poolTag + 2) != 0) && (*(poolTag + 3) != 0))
			{
				allocation.PoolTag = "";
				allocation.PoolTag += *poolTag;
				allocation.PoolTag += *(poolTag + 1);
				allocation.PoolTag += *(poolTag + 2);
				allocation.PoolTag += *(poolTag + 3);
				allocation.PoolTag += '\x0';
			}
		}
	}

	if (Tag && Allocated)
	{
		for (int i = 0; i < strlen(Tag); i++)
		{
			if (allocation.PoolTag[i] != Tag[i])
			{
				goto Exit;
			}
		}
	}

	spaces = "";
	for (int i = allocation.Size; i < 0x10000; i *= 0x10)
	{
		spaces += " ";
	}
	spaces += '\x0';

	type = "";
	if (Type == ALLOCATION_TYPE::Vs)
	{
		type = "Vs";
	}
	else if (Type == ALLOCATION_TYPE::Lfh)
	{
		type = "Lfh";
	}
	else if (Type == ALLOCATION_TYPE::Big)
	{
		type = "Big";
	}
	else if (Type == ALLOCATION_TYPE::Large)
	{
		type = "Large";
	}
	else
	{
		type = "Unknown";
	}
	type += '\x0';

	g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "%s 0x%p   0x%x %s   %s   %s   %s\n",
		Highlight ? "*" : " ",
		allocation.Address,
		allocation.Size,
		spaces.c_str(),
		Type == ALLOCATION_TYPE::Lfh? allocation.Allocated ? "(Free)" : "(Busy)":allocation.Allocated ? "(Allocated)" : "(Free)     ",
		allocation.PoolTag.c_str(),
		type.c_str());
	if (Allocations)
	{
		Allocations->push_back(allocation);
	}

Exit:
	if (poolHeader != nullptr)
	{
		VirtualFree(poolHeader, NULL, MEM_RELEASE);
	}
	return result;
}

/*
	Parse the dump for information about a large pool allocation
	and add its information to the provided allocations list.
*/
BOOLEAN
ParseLargePoolAlloc(
	_In_ ULONG64 Address,
	_In_ BOOLEAN LargePool,
	_Out_opt_ std::list<ALLOC>* Allocations,
	_In_opt_ PCSTR Tag
)
{
	HRESULT result;
	PVOID heapVamgrRange;
	BOOLEAN bitmapResult;
	ULONG elementSizeShift;
	ULONG64 baseAddress;
	ULONG64 bitIndex;
	ULONG64 userBitmapValue;
	ULONG64 elementSizeShiftAddress;
	ULONG64 baseAddressAddress;
	ULONG64 userBitmapAddress;
	ULONG64 numberOfBytes;
	ALLOC allocation;

	heapVamgrRange = nullptr;
	bitmapResult = FALSE;
	numberOfBytes = 0;

	//
	// Allocate memoryfor the VaMgrRange and for the poolTrackerBigPages structures.
	//
	heapVamgrRange = VirtualAlloc(NULL, g_VamgrRangeSize, MEM_COMMIT, PAGE_READWRITE);
	if (heapVamgrRange == nullptr)
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "Failed to allocate memory\n");
		result = S_FALSE;
		goto Exit;
	}

	//
	// Read the values we need
	//
	elementSizeShiftAddress = g_PoolState +
		g_HeapManagerStateOffsets.HeapManagerOffset +
		g_HpHeapManagerOffsets.VaMgrOffset +
		g_VamgrCtxOffsets.VaSpaceOffset +
		g_VamgrVaspaceOffsets.VaRangeArrayOffset +
		g_RtlSparseArrayOffsets.ElementSizeShiftOffset;
	baseAddressAddress = g_PoolState +
		g_HeapManagerStateOffsets.HeapManagerOffset
		+ g_HpHeapManagerOffsets.VaMgrOffset +
		g_VamgrCtxOffsets.VaSpaceOffset +
		g_VamgrVaspaceOffsets.BaseAddressOffset;
	userBitmapAddress = g_PoolState +
		g_HeapManagerStateOffsets.HeapManagerOffset +
		g_HpHeapManagerOffsets.VaMgrOffset +
		g_VamgrCtxOffsets.VaSpaceOffset +
		g_VamgrVaspaceOffsets.VaRangeArrayOffset +
		g_RtlSparseArrayOffsets.BitmapOffset +
		g_RtlCsparseBitmapOffsets.UserBitmapOffset;

	if ((!SUCCEEDED(g_DataSpaces->ReadVirtual(elementSizeShiftAddress, &elementSizeShift, sizeof(elementSizeShift), nullptr))) ||
		(!SUCCEEDED(g_DataSpaces->ReadVirtual(baseAddressAddress, &baseAddress, sizeof(baseAddress), nullptr))) ||
		(!SUCCEEDED(g_DataSpaces->ReadVirtual(userBitmapAddress, &userBitmapValue, sizeof(userBitmapValue), nullptr))))
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Calculate the bitIndex for the address and read the VaMgrRange structure for it.
	//
	bitIndex = ((Address - baseAddress) >> 20) << elementSizeShift;
	result = g_DataSpaces->ReadVirtual(userBitmapValue + bitIndex,
		heapVamgrRange,
		g_VamgrRangeSize,
		nullptr);

	if ((!SUCCEEDED(result)) || (heapVamgrRange == nullptr))
	{
		goto Exit;
	}

	//
	// Check if the block is marked as allocated.
	//
	if (_bittest64((LONG64*)((ULONG64)heapVamgrRange + g_VamgrRangeOffsets.AllocatedOffset), 0) == 1)
	{
		bitmapResult = TRUE;
	}

	//
	// Find the tag in PoolBigPageTable.
	//

	numberOfBytes = GetTagFromBigPagesTable(Address, &allocation.PoolTag);
	//
	// Either the read operation failed or the entry for this block is empty.
	// In both cases, this doesn't seem like a valid block so don't add it.
	//
	if (numberOfBytes == 0)
	{
		goto Exit;
	}

	PrintInfoForSingleBlock(Address,
		numberOfBytes,
		bitmapResult,
		LargePool ? ALLOCATION_TYPE::Large : ALLOCATION_TYPE::Big,
		0,
		FALSE,
		Tag,
		Allocations);

	//
	// Add this allocation to the list.
	//
	if (Allocations != nullptr)
	{
		allocation.Address = Address;
		allocation.Size = numberOfBytes;
		if (LargePool)
		{
			allocation.Type = ALLOCATION_TYPE::Large;
		}
		else
		{
			allocation.Type = ALLOCATION_TYPE::Big;
		}
		allocation.Allocated = bitmapResult;
		Allocations->push_back(allocation);
	}

Exit:
	if (heapVamgrRange != nullptr)
	{
		VirtualFree(heapVamgrRange, NULL, MEM_RELEASE);
	}
	return bitmapResult;
}

/*
	Iterates over the tree of large pool allocations and prints them.
	Can receive a pool tag and will only print blocks with a matching tag.

	Allocations remained from PoolViewer and is not used. Can be ignored for now.
*/
VOID IterateOverLargeAllocTree(
	_In_ ULONG64 CurrentEntryAddr,
	_In_ ULONG64 RootAddrForDecode,
	_In_ BOOLEAN Encoded,
	_In_ std::list<ALLOC>* Allocations,
	_In_opt_ PCSTR Tag
)
{
	PVOID heapLargeAlloc;
	ULONG64 right;
	ULONG64 left;
	ULONG64 va;
	heapLargeAlloc = nullptr;
	heapLargeAlloc = VirtualAlloc(NULL, g_HeapLargeAllocDataSize, MEM_COMMIT, PAGE_READWRITE);
	if (heapLargeAlloc == nullptr)
	{
		goto Exit;
	}

	if (Encoded)
	{
		//
		// In some cases the metadata is encoded.
		// If it is, we need to decode each entry by XORing its address with the tree base.
		//
		CurrentEntryAddr = CurrentEntryAddr ^ RootAddrForDecode;
	}

	//
	// Read the HEAP_LARGE_ALLOC_DATA structure that root points to.
	// If points to the TreeNode field of the structure so we have to do a sort
	// of CONTAINING_RECORD.
	//
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(CurrentEntryAddr, heapLargeAlloc, g_HeapLargeAllocDataSize, nullptr)))
	{
		goto Exit;
	}
	//
	// Get information about current entry and add to Allocations list
	//
	va = *(ULONG64*)((ULONG64)heapLargeAlloc + g_HeapLargeAllocDataOffsets.VirtualAddressOffset);
	ParseLargePoolAlloc(va, true, Allocations, Tag);

	//
	// Get children
	//
	right = *(ULONG64*)((ULONG64)heapLargeAlloc + g_HeapLargeAllocDataOffsets.TreeNodeOffset + g_RtlBalancedNodeOffsets.RightOffset);
	left = *(ULONG64*)((ULONG64)heapLargeAlloc + g_HeapLargeAllocDataOffsets.TreeNodeOffset + g_RtlBalancedNodeOffsets.LeftOffset);

	if (left != 0)
	{
		IterateOverLargeAllocTree(left, RootAddrForDecode, Encoded, Allocations, Tag);
	}
	if (right != 0)
	{
		IterateOverLargeAllocTree(right, RootAddrForDecode, Encoded, Allocations, Tag);
	}
Exit:
	if (heapLargeAlloc != nullptr)
	{
		VirtualFree(heapLargeAlloc, NULL, MEM_RELEASE);
	}
}

/*
	Finds all large pool blocks and prints them.
	Can receive a pool tag and will only print blocks with a matching tag.

	Allocations remained from PoolViewer and is not used. Can be ignored for now.
*/
VOID ParseLargePages(
	_In_ ULONG64 Heap,
	_In_ std::list<ALLOC>* Allocations,
	_In_opt_ PCSTR Tag
)
{
	PVOID largeAllocMetadata;
	ULONG64 largeAllocMedatadaAddr;
	ULONG64 heapLargeAllocAddr;
	ULONG64 root;
	CHAR encoded;
	largeAllocMetadata = nullptr;
	//
	// Read LargeAllocMetadata field from heap.
	// It contains an RTL_RB_TREE so allocate memory for that.
	//
	largeAllocMetadata = VirtualAlloc(NULL, g_RtlRbTreeSize, MEM_COMMIT, PAGE_READWRITE);
	if (largeAllocMetadata == nullptr)
	{
		goto Exit;
	}
	largeAllocMedatadaAddr = Heap + g_SegmentHeapOffsets.LargeAllocMetadataOffset;
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(largeAllocMedatadaAddr, largeAllocMetadata, g_RtlRbTreeSize, nullptr)))
	{
		goto Exit;
	}
	//
	// If root is empty that means there are no large allocations in this heap.
	//
	root = *(ULONG64*)((ULONG64)largeAllocMetadata + g_RtlRbTreeOffsets.RootOffset);
	if (root == 0)
	{
		goto Exit;
	}

	//
	// Check if the tree is encoded
	//
	heapLargeAllocAddr = root - g_HeapLargeAllocDataOffsets.TreeNodeOffset;
	encoded = _bittest64((LONG64*)((ULONG64)largeAllocMetadata + g_RtlRbTreeOffsets.EncodedOffset), 0);
	IterateOverLargeAllocTree(heapLargeAllocAddr, largeAllocMedatadaAddr, encoded, Allocations, Tag);

Exit:
	if (largeAllocMetadata != nullptr)
	{
		VirtualFree(largeAllocMetadata, NULL, MEM_RELEASE);
	}
}

/*
	Parse the supplied LFH subsegment to get all the pool blocks in it
	and print them as needed.

	@param[in] LfhSubsegment - Beginning of an LFH subsegment
	@param[in] SubsegmentEnd - End of an LFH subsegment
	@param[in] Allocations - Left over from PoolViewer, can be ignored for now.
	@param[in,opt] Address - A specific address to search for.
		If supplied, only blocks in the page containing Address will be printed
		and the block containing the requested address will be marked with *.
	@param[in,opt] Tag - Optional pool tag. If supplied, only blocks with a matching
		pool tag will be printed.
*/
HRESULT
FindPoolBlocksInLfhSubsegment(
	_In_ PVOID LfhSubsegment,
	_In_ PVOID SubsegmentEnd,
	_In_opt_ std::list<ALLOC>* Allocations,
	_In_opt_ PVOID Address,
	_In_opt_ PCSTR Tag
)
{
	HRESULT result;
	ULONG encodedData;
	ULONG lfhSubsegmentEncodedData;
	USHORT lfhBlockSize;
	USHORT lfhFirstEntryOffset;
	ULONG64 currentAddress;
	UCHAR bitmapResult;
	ULONG offsetInSubseg;
	ULONG indexInBitmap;
	PVOID bitmap;
	ALLOC allocation;
	ULONG64 encodedDataAddress;
	ULONG64 validBase;
	ULONG validSize;
	BOOLEAN highlight;

	bitmap = nullptr;
	result = S_OK;

	//
	// It is possible that only some pages in the subsegment are used,
	// so check that actual used size and don't try to access invalid pages.
	//
	result = g_DataSpaces->GetValidRegionVirtual(
		(ULONG64)LfhSubsegment,
		(ULONG)((ULONG64)SubsegmentEnd - (ULONG64)LfhSubsegment),
		&validBase,
		&validSize);

	/*if ((ULONG64)LfhSubsegment != validBase)
	{
		result = S_FALSE;
		goto Exit;
	}

	if ((ULONG64)SubsegmentEnd - (ULONG64)LfhSubsegment > validSize)
	{
		SubsegmentEnd = (PVOID)(validBase + validSize);
	}*/

	//
	// Read the HEAP_PAGE_SUBSEGMENT encodedData.
	//
	encodedDataAddress = (ULONG64)LfhSubsegment + g_lfhSubsegmentOffsets.BlockOffsets + g_lfhSubsegmentEncodedEffsets.EncodedData;
	if (!SUCCEEDED(ReadData(encodedDataAddress, sizeof(encodedData), (PVOID)&encodedData)))
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Decode encodedData and get BlockSize and FirstEntryOffset
	// that we need to parse the subsegment.
	//
	lfhSubsegmentEncodedData = encodedData ^ (ULONG)g_LfhKey ^ ((ULONG)((ULONG_PTR)LfhSubsegment >> 0xc));
	lfhBlockSize = (lfhSubsegmentEncodedData + g_lfhSubsegmentEncodedEffsets.BlockSize) & 0x0000ffff;
	lfhFirstEntryOffset = *((USHORT*)((ULONG64)&lfhSubsegmentEncodedData + g_lfhSubsegmentEncodedEffsets.FirstBlockOffset)) & 0xffff;

	//
	// We have a start address and the size of the allocations, we can
	// iterate over them until we find the start block of our allocation
	//
	currentAddress = (ULONG64)LfhSubsegment + lfhFirstEntryOffset;
	bitmap = VirtualAlloc(NULL, lfhFirstEntryOffset - g_lfhSubsegmentOffsets.BlockBitmap, MEM_COMMIT, PAGE_READWRITE);
	if (bitmap == nullptr)
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Read the bitmap here so we don't need to read a part of it for every allocation
	//
	if (!SUCCEEDED(ReadData((ULONG64)LfhSubsegment + g_lfhSubsegmentOffsets.BlockBitmap,
		lfhFirstEntryOffset - g_lfhSubsegmentOffsets.BlockBitmap,
		bitmap)))
	{
		result = S_FALSE;
		goto Exit;
	}

	while (currentAddress + lfhBlockSize <= (ULONG_PTR)SubsegmentEnd)
	{
		highlight = FALSE;
		//
		// It looks like blocks actually don't cross page boundaries, not really sure why
		//
		if (0x1000 - (currentAddress & 0xfff) >= lfhBlockSize)
		{
			if ((!Address) || (currentAddress > (ULONG64)ALIGN_DOWN_POINTER_BY(Address, 0x1000)))
			{
				//
				// The LFH subsegment contains a bitmap where each block is marked by 2 bits stating its allocation status.
				// To check that bit:
				//   1. Get the allocation offset in the subsegment (remember to include FirstEntryOffset)
				//   2. Get the index by dividing the offset with the block size and multiplying by 2 (2 bits for each block)
				//   3. Read the correct byte from the bitmap (byte = index / 8) and check the correct bit (index % 8)
				//
				offsetInSubseg = currentAddress - lfhFirstEntryOffset - (ULONG64)LfhSubsegment;
				indexInBitmap = (offsetInSubseg / lfhBlockSize) * 2;
			//	bitmapResult = *(CHAR*)((ULONG64)bitmap + (indexInBitmap / 8));
				bitmapResult = BitTest64((const LONG64 *)bitmap, indexInBitmap);

				if (Address && (currentAddress > (ULONG64)ALIGN_UP_POINTER_BY(Address, 0x1000)))
				{
					break;
				}
				if (Address)
				{
					if ((currentAddress <= (ULONG64)Address) &&
						(currentAddress + lfhBlockSize > (ULONG64)Address))
					{
						highlight = TRUE;
					}
				}
				PrintInfoForSingleBlock(
					currentAddress,
					lfhBlockSize,
					_bittest64((LONG64*)(&bitmapResult), indexInBitmap % 8),
					ALLOCATION_TYPE::Lfh,
					currentAddress,
					highlight,
					Address ? 0 : Tag,
					Allocations);
			}
		}
		//
		// Advance to the next block.
		//
		currentAddress = currentAddress + lfhBlockSize;
	}
Exit:
	if (bitmap != nullptr)
	{
		VirtualFree(bitmap, NULL, MEM_RELEASE);
	}
	return result;
}

/*
	Prints information for a VS pool block, optionally filtered by tag.

	@param[in] ChunkHeader - Address of the chunk header belonging to the pool block
	@param[in] BlockSize - Size of the pool block
	@param[in] Allocated- Is the block allocated or free
	@param[in,opt] Tag - Optional pool tag. If supplied, only blocks with a matching
		pool tag will be printed.
*/
VOID
PrintInfoForAllVsAllocs(
	_In_ ULONG64 ChunkHeader,
	_In_ ULONG BlockSize,
	_In_ BOOLEAN Allocated,
	_In_opt_ PCSTR Tag,
	_In_opt_ std::list<ALLOC>* Allocations
)
{
	ULONG64 currentAddress;
	ULONG currentSize;

	if (!Allocated)
	{
		return;
	}
	if (BlockSize > 0x1000)
	{
		//
		// No pool header
		//
		currentAddress = ChunkHeader + g_vsChunkHeaderSize;
		currentSize = BlockSize - g_vsChunkHeaderSize;
	}
	else if ((0x1000 - (ChunkHeader & 0xfff) >= BlockSize) ||
		(0x1000 - (ChunkHeader & 0xfff) <= (g_vsChunkHeaderSize + g_poolHeaderSize)))
	{
		//
		// Allocation data doesn't cross page boundaries, so if UnsafeSize is
		// larger than the space that's left until the end of the page, this is
		// not an actual allocation so move on to the next one.
		// But there are blocks where the chunk header + pool header are right
		// before page break and the data is immediately after and those seem fine.
		//
		currentAddress = ChunkHeader + g_vsChunkHeaderSize + g_poolHeaderSize;
		currentSize = BlockSize - g_vsChunkHeaderSize - g_poolHeaderSize;
	}
	else
	{
		return;
	}
	PrintInfoForSingleBlock(
		currentAddress,
		currentSize,
		Allocated,
		ALLOCATION_TYPE::Vs,
		ChunkHeader + g_vsChunkHeaderSize,
		FALSE,
		Tag,
		Allocations);
}

/*
	Prints information for a VS pool block only if matching the requested address.

	@param[in] ChunkHeader - Address of the chunk header belonging to the pool block
	@param[in] BlockSize - Size of the pool block
	@param[in] Allocated- Is the block allocated or free
	@param[in] Address - Address requested by the user.
		Pool block will only be printed if it's in the page containing Address.
		The pool block that Address belongs to will be marked with *.
*/
VOID
PrintInfoForVSAddress(
	_In_ ULONG64 ChunkHeader,
	_In_ ULONG BlockSize,
	_In_ BOOLEAN Allocated,
	_In_ ULONG64 Address,
	_In_opt_ std::list<ALLOC>* Allocations
)
{
	BOOLEAN highlight;

	//
	// Print all pool blocks in the page the allocation is in
	//
	if (((ChunkHeader + g_vsChunkHeaderSize + g_poolHeaderSize) & ~0xfff) == (Address & ~0xfff))
	{
		highlight = FALSE;
		if ((ChunkHeader <= Address) &&
			(ChunkHeader + BlockSize > Address))
		{
			highlight = TRUE;
		}
		PrintInfoForSingleBlock(
			ChunkHeader + g_vsChunkHeaderSize + g_poolHeaderSize,
			BlockSize - g_vsChunkHeaderSize - g_poolHeaderSize,
			Allocated,
			ALLOCATION_TYPE::Vs,
			ChunkHeader + g_vsChunkHeaderSize,
			highlight,
			0,
			Allocations);
	}
}

/*
	Prints information for pool blocks found in a VS subsegment.

	@param[in] StartAddress - Start address of the subsegment
	@param[in] EndAddress - End address of the subsegment
	@param[in,opt] Address- Optional address to search for
	@param[in,opt] Tag - Optional pool tag. If supplied, only blocks with a matching
		pool tag will be printed.
*/
VOID
PrintInfoForVsSubsegment(
	_In_ ULONG64 StartAddress,
	_In_ ULONG64 EndAddress,
	_In_opt_ PVOID Address,
	_In_opt_ PCSTR Tag,
	_In_opt_ std::list<ALLOC>* Allocations
)
{
	ALLOC allocation;
	ULONG64 chunkHeaderKernelAddress;
	PVOID chunkHeader = nullptr;
	ULONG64 vsChunkHeaderSizes;
	ULONG64 headerBits;
	ULONG unsafeSize;
	BYTE allocated;
	ULONG64 currentAddress;
	ULONG currentSize;

	ULONG64 validBase;
	ULONG validSize;

	chunkHeaderKernelAddress = StartAddress;
	currentAddress = 0;
	currentSize = 0;

	chunkHeader = VirtualAlloc(NULL, g_vsChunkHeaderSize, MEM_COMMIT, PAGE_READWRITE);
	if (chunkHeader == nullptr)
	{
		goto Exit;
	}
	//
	// Walk the pool chunks in the subsegment and get information for each one.
	//
	do
	{
		//
		// Read the current chunk header.
		//
		if (!SUCCEEDED(ReadData((ULONG64)chunkHeaderKernelAddress, g_vsChunkHeaderSize, chunkHeader)))
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadData chunkHeaderKernelAddress 0x%p\n", __FUNCTIONW__, __LINE__, chunkHeaderKernelAddress);
			goto Exit;
		}

		//
		// Get the VS_CHUNK_HEADER_SIZES from the chunk header.
		//
		vsChunkHeaderSizes = *(ULONG64*)((ULONG64)chunkHeader + g_VsChunkHeaderOffsets.SizesOffset);
		headerBits = *(ULONG64*)(&vsChunkHeaderSizes + g_VsChunkHeaderSizeOffsets.HeaderBitsOffset);
		if (headerBits == 0)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] headerBits 0 chunkHeaderKernelAddress 0x%p\n", __FUNCTIONW__, __LINE__, chunkHeaderKernelAddress);
			goto Exit;
		}

		//
		// Decode the VS_CHUNK_HEADER_SIZES structure and get UnsafeSize
		// and Allocated fields from it.
		//
		headerBits ^= (ULONG_PTR)chunkHeaderKernelAddress ^ g_HeapKey;
		unsafeSize = (USHORT)((*(ULONG64*)((ULONG64)&headerBits
			+ g_VsChunkHeaderSizeOffsets.UnsafeSizeOffset)) >> 16) * 0x10; /* bit position: 16 */
		allocated = (BYTE)((*(ULONG64*)((ULONG64)&headerBits
			+ g_VsChunkHeaderSizeOffsets.AllocatedOffset)) >> 16); /* bit position: 16 */


	//	g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] PrintInfoForVsSubsegment %p\n", __FUNCTIONW__, __LINE__, chunkHeaderKernelAddress);
		if (Address)
		{
			PrintInfoForVSAddress(chunkHeaderKernelAddress, unsafeSize, allocated, (ULONG64)Address, Allocations);
		}
		else
		{
			PrintInfoForAllVsAllocs(chunkHeaderKernelAddress, unsafeSize, allocated, Tag, Allocations);
		}

	Next:
		//
		// Advance to the next chunk header.
		//
		chunkHeaderKernelAddress = chunkHeaderKernelAddress + unsafeSize;
	} while ((ULONG_PTR)chunkHeaderKernelAddress < EndAddress);

Exit:
	if (chunkHeader != nullptr)
	{
		VirtualFree(chunkHeader, NULL, MEM_RELEASE);
	}
	return;
}

/*
	Parses the supplied VS subsegment to get all the pool blocks in it
	and add them to the Allocations list.
*/
HRESULT
FindPoolBlocksInVsSubsegment(
	_In_ PVOID VsSubsegment,
	_In_ PVOID SubsegmentEnd,
	_In_opt_ std::list<ALLOC>* Allocations,
	_In_opt_ PVOID Address,
	_In_opt_ PCSTR Tag
)
{
	HRESULT result;
	USHORT signature;
	USHORT size;
	PVOID chunkHeader;
	ULONG64 chunkHeaderKernelAddress;
	PVOID poolHeader;
	ALLOC allocation;

	ULONG64 validBase;
	ULONG validSize;

	chunkHeader = nullptr;
	poolHeader = nullptr;
	result = S_OK;

	//
	// First validate that this really is a VS subsegment by checking if
	// vsSubsegment->Signature ^ 0x2BED == vsSubsegment.Size
	//
	result = g_DataSpaces->GetValidRegionVirtual((ULONG64)VsSubsegment, g_vsSubsegmentSize, &validBase, &validSize);
	if ((!SUCCEEDED(result)) || (validBase != (ULONG64)VsSubsegment))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Invalid region at subsegment 0x%p\n", __FUNCTIONW__, __LINE__, VsSubsegment);
		result = S_FALSE;
		goto Exit;
	}

	if ((!SUCCEEDED(g_DataSpaces->ReadVirtual((ULONG64)VsSubsegment + g_VsSubsegmentOffsets.SignatureOffset, &signature, sizeof(signature), nullptr))) ||
		(!SUCCEEDED(g_DataSpaces->ReadVirtual((ULONG64)VsSubsegment + g_VsSubsegmentOffsets.SizeOffset, &size, sizeof(size), nullptr))))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed to read data\n", __FUNCTIONW__, __LINE__);
		result = S_FALSE;
		goto Exit;
	}

	if ((signature ^ 0x2BED) != size)
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Incorrect signature at subsegment 0x%p\n", __FUNCTIONW__, __LINE__, VsSubsegment);
		result = S_FALSE;
		goto Exit;
	}

	//
	// Get the VS chunk header that is right after the HEAP_VS_SUBSEGMENT structure.
	//
	chunkHeaderKernelAddress = (ULONG64)VsSubsegment + g_vsSubsegmentSize;
	chunkHeaderKernelAddress = (ULONG64)ALIGN_UP_POINTER_BY((PVOID)chunkHeaderKernelAddress, 0x10);

	PrintInfoForVsSubsegment(chunkHeaderKernelAddress, (ULONG_PTR)SubsegmentEnd, Address, Tag, Allocations);

Exit:
	return result;
}

/*
	Parses a pool descriptor and prints the requested pool blocks in it.
	Finds the type for the descriptor and calls an appropriate function
	for the type.

	@param[in] SegmentAddress - Kernel address of the HEAP_PAGE_SEGMENT being parsed
	@param[in] HeapPageSeg - Local buffer containing the data from SegmentAddress
		HEAP_PAGE_SEGMENT is a large structure to we read it once and pass the local copy around
		for each descriptor.
	@param[in] CurrentDescriptorIndex - Index of the descriptor to parse
	@param[in] UnitShift - Number of shifts needed to find the basic unit handled by this segment
		Can be 0xC of 0x10 depending on segment type.
	@param[in,opt] Allocations - Left over from PoolViewer, can be ignored for now
	@param[in,opt] Address- Optional address to search for
	@param[in,opt] Tag - Optional pool tag. If supplied, only blocks with a matching
		pool tag will be printed.
*/
ULONG
GetDataForDescriptor(
	_In_ ULONG64 SegmentAddress,
	_In_ PVOID HeapPageSeg,
	_In_ ULONG CurrentDescriptorIndex,
	_In_ UCHAR UnitShift,
	_In_opt_ std::list<ALLOC>* Allocations,
	_In_opt_ PVOID Address,
	_In_opt_ PCSTR Tag
)
{
	HRESULT result;
	UCHAR rangeFlags;
	BOOLEAN allocated;
	ULONG64 descArrayEntry;
	UCHAR unitSize;
	ULONG64 segmentStart = 0;
	ULONG64 segmentEnd = 0;

	result = S_OK;
	//
	// Get the current DescArray entry and get the UnitSize and RangeFlags fields from it.
	//
	descArrayEntry = (ULONG64)HeapPageSeg + g_PageSegmentOffsets.DescArrayOffset + (g_RangeDescriptorSize * CurrentDescriptorIndex);
	ULONG64 descArrayEntryRaw = (ULONG64)SegmentAddress + g_PageSegmentOffsets.DescArrayOffset + (g_RangeDescriptorSize * CurrentDescriptorIndex);
	unitSize = *(UCHAR*)(descArrayEntry + g_PageRangeDescriptorOffsets.UnitSizeOffset);
	rangeFlags = *(UCHAR*)(descArrayEntry + g_PageRangeDescriptorOffsets.RangeFlagsOffset);
	g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Unit size %x %p %p %p %p\n", __FUNCTIONW__, __LINE__, unitSize, SegmentAddress, descArrayEntryRaw, CurrentDescriptorIndex, Address);
	ULONG64  SignaturePageSeg = *(ULONG64*)((ULONG64)HeapPageSeg + g_PageSegmentOffsets.SignatureOffset);
	ULONG64  heapctx = (ULONG64)SegmentAddress ^ g_HeapKey ^ SignaturePageSeg;

	if (heapctx == 0)
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Invalid SegmentAddress Not Match %p %p \n", __FUNCTIONW__, __LINE__, heapctx, SegmentAddress);
		return 1;
	}
	ULONG64 VsContextOffsetAddr = heapctx + g_SegContextOffsets.VsContextOffset;
	ULONG64 LfhContextOffsetAddr = heapctx + g_SegContextOffsets.LfhContextOffset;

	ULONG64 VsContextObj = 0;
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(VsContextOffsetAddr, &VsContextObj, 8, nullptr)))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual VsContextOffsetAddr  %p \n", __FUNCTIONW__, __LINE__, VsContextOffsetAddr);
		return 1;
	}
	ULONG64 LfhContextObj = 0;
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(LfhContextOffsetAddr, &LfhContextObj, 8, nullptr)))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual LfhContextOffsetAddr  %p \n", __FUNCTIONW__, __LINE__, VsContextOffsetAddr);
		return 1;
	}
	g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Heap:=> dt _HEAP_SEG_CONTEXT  %p ;dt _HEAP_VS_CONTEXT %p ;dt _HEAP_LFH_CONTEXT %p\n", __FUNCTIONW__, __LINE__, heapctx, VsContextObj, LfhContextObj);

	if (unitSize == 0&& rangeFlags&1==0)
	{		

		LIST_ENTRY64 subsegentry = { 0 };
		//ULONG64 SubsegmentListAddr = VsContextObj + g_HeapVsContext.SubsegmentListOffset;
		//ULONG64 SubsegmentListAddr = (ULONG64)Address & 0xfffffffffffff000;
		ULONG64 SubsegmentListAddr = (ULONG64)SegmentAddress + 0x2000;
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(SubsegmentListAddr, &subsegentry, sizeof(LIST_ENTRY64), nullptr)))
		{

			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual SubsegmentListAddr  %p \n", __FUNCTIONW__, __LINE__, SubsegmentListAddr);

			return 1;
		}


		ULONG64   ctxsubsegblkaddr = subsegentry.Blink ^ SubsegmentListAddr;

		LIST_ENTRY64 ctxsubsegblk = { 0 };
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(ctxsubsegblkaddr, &ctxsubsegblk, sizeof(LIST_ENTRY64), nullptr)))
		{

			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual ctxsubsegblkaddr  %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr);

			return 1;
		}

		if ((ULONG64)((ULONG64)ctxsubsegblk.Flink ^ (ULONG64)ctxsubsegblkaddr) != SubsegmentListAddr)
		{

			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Decode ListEntry Failed %p %p %p %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr, SubsegmentListAddr, ctxsubsegblk.Flink, ctxsubsegblk.Blink);

			return 1;

		}


		


		ctxsubsegblkaddr = subsegentry.Flink ^ SubsegmentListAddr;
		ULONG64 AddressEnd = (ULONG64)SegmentAddress + 0x100000;
		int fetchidx = 0;
		int fetchidxvec = 0;
		std::vector<ULONG64>::iterator it;
		std::vector<ULONG64> segvec;
		for (ULONG64 SubsegmentListtmp = ctxsubsegblkaddr; SubsegmentListtmp < AddressEnd; fetchidx++)
		{


			LIST_ENTRY64 ctxsubsegblktmp = { 0 };
			if (!SUCCEEDED(g_DataSpaces->ReadVirtual(SubsegmentListtmp, &ctxsubsegblktmp, sizeof(LIST_ENTRY64), nullptr)))
			{

				g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual SubsegmentListtmp  %p \n", __FUNCTIONW__, __LINE__, SubsegmentListtmp);

				return 1;
			}
			//g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] fetch SubsegmentList %p %p %p \n", __FUNCTIONW__, __LINE__, SubsegmentListtmp, ctxsubsegblktmp.Blink, ctxsubsegblktmp.Flink);





			ctxsubsegblkaddr = ctxsubsegblktmp.Blink ^ SubsegmentListtmp;
			if (!SUCCEEDED(g_DataSpaces->ReadVirtual(ctxsubsegblkaddr, &ctxsubsegblk, sizeof(LIST_ENTRY64), nullptr)))
			{

				g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual ctxsubsegblkaddr  %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr);

				return 1;
			}


			if ((ULONG64)((ULONG64)ctxsubsegblk.Flink ^ (ULONG64)ctxsubsegblkaddr) != SubsegmentListtmp)
			{

				g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Decode ListEntry Failed %p %p %p %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr, SubsegmentListtmp, ctxsubsegblktmp.Flink, ctxsubsegblktmp.Blink);

				return 1;

			}

			it = std::find(segvec.begin(), segvec.end(), SubsegmentListtmp);
			if (it == segvec.end())
			{
				segvec.push_back(SubsegmentListtmp);
				ULONG64 SubsegmentListNext = ctxsubsegblktmp.Flink ^ SubsegmentListtmp;


				SubsegmentListtmp = SubsegmentListNext;
			}
			else
			{
				break;

			}

		}
		std::sort(segvec.begin(), segvec.end());


		for (it = segvec.begin(); it != segvec.end(); fetchidxvec++)
		{
			ULONG64 SubsegmentListtmp = *it;
			it++;
			if (it != segvec.end())
			{
				ULONG64 SubsegmentListNext = *it;

				//g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] iterator  %p %p \n", __FUNCTIONW__, __LINE__, SubsegmentListtmp, SubsegmentListNext);
				if ((ULONG64)Address >= SubsegmentListtmp && (ULONG64)Address <= SubsegmentListNext)
				{

					//g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] FindPoolBlocksInVsSubsegment  %p %p %p %p \n", __FUNCTIONW__, __LINE__, segmentStart, SubsegmentListNext, Address, segmentEnd);
					segmentStart = (ULONG_PTR)SubsegmentListtmp;
					segmentEnd = (ULONG_PTR)((ULONG64)Address >> UnitShift << UnitShift) + (2 << UnitShift);
					g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] dt _HEAP_VS_SUBSEGMENT %p \n", __FUNCTIONW__, __LINE__, segmentStart);
					//g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] FindPoolBlocksInVsSubsegment  %p %p %p %p\n", __FUNCTIONW__, __LINE__, Address, SubsegmentListtmp, SubsegmentListNext, segmentEnd);
					result = FindPoolBlocksInVsSubsegment(
						(PVOID)segmentStart,
						(PVOID)segmentEnd,
						Allocations,
						Address,
						Tag);
					break;
				}
			}
		}

		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Pool fetchidx  %p %x %x\n", __FUNCTIONW__, __LINE__, Address, fetchidx, fetchidxvec);
		return 0;


		//return 1;
	}


	//
	// Calculate the start and end addresses of this subsegment.
	//
	segmentStart = (ULONG_PTR)SegmentAddress + ((ULONG64)CurrentDescriptorIndex * 1 << UnitShift);
	segmentEnd = (ULONG_PTR)SegmentAddress + (((ULONG64)CurrentDescriptorIndex + unitSize) * 1 << UnitShift);

	//
	// RangeFlags 0 / 2 = area not currently used by any allocations (bottom bit not set)
	// If we requested information for a specific address and it's in the middle of a subsegment,
	// call this function again with the index of the beginning of the subsegment.
	// If we do not care about a specific address we will iterate all subsegments anyway
	// so we'll ignore descriptors that are in the middle of a subsegment.
	//
	if (Address && (rangeFlags == 1))
	{
		segmentEnd = ((ULONG64)Address & 0xfffffffffffff000) + 0x1000;
		ULONG64 SubsegmentListAddr = ((ULONG64)Address & 0xfffffffffffff000) +0x1000;
		while (SegmentAddress< SubsegmentListAddr)
		{
			LIST_ENTRY64 subsegentry = { 0 };

			SubsegmentListAddr -= 0x1000;
			if (!SUCCEEDED(g_DataSpaces->ReadVirtual(SubsegmentListAddr, &subsegentry, sizeof(LIST_ENTRY64), nullptr)))
			{

			//	g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual SubsegmentListAddr  %p \n", __FUNCTIONW__, __LINE__, SubsegmentListAddr);

				continue;
			}


			ULONG64   ctxsubsegblkaddr = subsegentry.Flink;

			LIST_ENTRY64 ctxsubsegblk = { 0 };
			if (!SUCCEEDED(g_DataSpaces->ReadVirtual(ctxsubsegblkaddr, &ctxsubsegblk, sizeof(LIST_ENTRY64), nullptr)))
			{

				//g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ReadVirtual ctxsubsegblkaddr  %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr);

				continue;
			}

			if ((ULONG64)ctxsubsegblk.Blink != SubsegmentListAddr)
			{

				   g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Decode ListEntry Failed %p %p %p %p \n", __FUNCTIONW__, __LINE__, ctxsubsegblkaddr, SubsegmentListAddr, ctxsubsegblk.Flink, ctxsubsegblk.Blink);
					continue;

			}

			
			
			segmentStart = SubsegmentListAddr;
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] dt _HEAP_LFH_SUBSEGMENT  %p \n", __FUNCTIONW__, __LINE__, segmentStart);
			
			result = FindPoolBlocksInLfhSubsegment(
				(PVOID)segmentStart,
				(PVOID)segmentEnd,
				Allocations,
				Address,
				Tag);
			break;
		}
		/*GetDataForDescriptor(SegmentAddress,
			HeapPageSeg,
			CurrentDescriptorIndex - unitSize,
			UnitShift,
			Allocations,
			Address,
			Tag);*/
		return 0;
	}
	else if ((rangeFlags != 0) && (rangeFlags != 2))
	{
		//
		// Check if descriptor is valid by comparing the signature with the magic signature value
		//
		if (*(ULONG*)(descArrayEntry + g_PageRangeDescriptorOffsets.TreeSignatureOffset) != 0xccddccdd)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Invalid descriptor signature\n", __FUNCTIONW__, __LINE__);
			return 1;
		}
		if (rangeFlags == 3)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ParseLargePoolAlloc\n", __FUNCTIONW__, __LINE__);
			//
			// Large pool
			//
			allocated = ParseLargePoolAlloc(
				segmentStart,
				false,
				Allocations,
				Tag);
		}
		else if (rangeFlags == 0xf)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] FindPoolBlocksInVsSubsegment\n", __FUNCTIONW__, __LINE__);
			//
			// VS subsegment
			//
			result = FindPoolBlocksInVsSubsegment(
				(PVOID)segmentStart,
				(PVOID)segmentEnd,
				Allocations,
				Address,
				Tag);
		}
		else if (rangeFlags == 0xb)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] FindPoolBlocksInLfhSubsegment\n", __FUNCTIONW__, __LINE__);
			//
			// Lfh subsegment
			//
			result = FindPoolBlocksInLfhSubsegment(
				(PVOID)segmentStart,
				(PVOID)segmentEnd,
				Allocations,
				Address,
				Tag);
		}
	}
	return unitSize;
}

/*
	Get all the allocations inside a HEAP_SEG_CONTEXT strcture.
*/
VOID
ParseSegContext(
	_In_ ULONG64 SegContext,
	_In_ std::list<ALLOC>* Allocations,
	_In_opt_ PCSTR Tag
)
{
	LIST_ENTRY listHead;
	PLIST_ENTRY nextEntry;
	LIST_ENTRY listEntry;
	PVOID heapPageSeg;
	CHAR firstDescriptorIndex;
	UCHAR unitSize;
	UCHAR unitShift;
	ULONG64 segment;

	heapPageSeg = nullptr;

	//
	// Read SegmentListHead, UnitShift and FirstDescriptorIndex fields from the segment.
	//
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(SegContext + g_SegContextOffsets.SegmentListHeadOffset, &listHead, sizeof(listHead), nullptr)) ||
		!SUCCEEDED(g_DataSpaces->ReadVirtual(SegContext + g_SegContextOffsets.UnitShiftOffset, &unitShift, sizeof(unitShift), nullptr)) ||
		!SUCCEEDED(g_DataSpaces->ReadVirtual(SegContext + g_SegContextOffsets.FirstDescriptorIndexOffset, &firstDescriptorIndex, sizeof(firstDescriptorIndex), nullptr)))
	{
		goto Exit;
	}
	nextEntry = listHead.Flink;
	listEntry.Flink = 0; /* This value will be overwritten */

	//
	// Allocate memory for the HEAP_PAGE_SEGMENT.
	//
	heapPageSeg = VirtualAlloc(NULL, g_HeapPageSegSize, MEM_COMMIT, PAGE_READWRITE);
	if (heapPageSeg == nullptr)
	{
		goto Exit;
	}

	//
	// Iterate over all the segments in this SegContext and get the pool blocks in them.
	//
	while (listEntry.Flink != listHead.Flink)
	{
		if ((ULONG64)nextEntry == SegContext + g_SegContextOffsets.SegmentListHeadOffset)
		{
			//
			// This means we hit the end of the list, no need for an error
			//
			goto Next;
		}

		//
		// We want to read the HEAP_PAGE_SEGMENT structures linked in this segContext
		// We basically need get the CONTAINING_RECORD for each list entry
		//
		segment = (ULONG64)nextEntry - g_PageSegmentOffsets.ListEntryOffset;
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(segment, heapPageSeg, g_HeapPageSegSize, nullptr)))
		{
			goto Next;
		}

		//
		// Each HEAP_PAGE_SEGMENT has 256 HEAP_PAGE_RANGE_DESCRIPTORs.
		// Walk over them and handle each separately.
		//
		for (int i = firstDescriptorIndex; i < 256; i++)
		{
			unitSize = GetDataForDescriptor(segment, heapPageSeg, i, unitShift, Allocations, 0, Tag);
			i += unitSize - 1;
		}
	Next:
		//
		// Move to the next HEAP_PAGE_SUBSEGMENT in the list.
		//
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual((ULONG64)nextEntry, &listEntry, sizeof(listEntry), nullptr)))
		{
			goto Exit;
		}
		nextEntry = listEntry.Flink;
	}
Exit:
	if (heapPageSeg != nullptr)
	{
		VirtualFree(heapPageSeg, NULL, MEM_RELEASE);
	}
	return;
}

/*
	Parse a SEGMENT_HEAP structure and get all the pool blocks in it.
*/
VOID
GetInformationForHeap(
	_In_ ULONG64 HeapAddress,
	_In_ std::list<ALLOC>* Allocations,
	_In_opt_ PCSTR Tag
)
{
	//
	// Parse SegContexts[0].
	//
	ParseSegContext(
		HeapAddress + g_SegmentHeapOffsets.SegContextsOffsets,
		Allocations,
		Tag);

	//
	// Parse SegContexts[1].
	//
	ParseSegContext(
		HeapAddress + g_SegmentHeapOffsets.SegContextsOffsets + g_SegContextSize,
		Allocations,
		Tag);

	//
	// Parse LargeAllocMetadata
	//
	ParseLargePages(HeapAddress, Allocations, Tag);
}

/*
	Find all SEGMENT_HEAP structures and parse them to get the pool blocks they manage.
*/
std::list<HEAP>
GetAllHeaps(
	_In_opt_ PCSTR Tag,
	_In_opt_ POOL_VIEW_FLAGS Flags
)
{
	HRESULT result;
	ULONG numberOfPools;
	ULONG64 heaps[4];
	ULONG64 specialHeaps[3];
	HEAP heap;
	POOL_TYPE heapNumberToPoolType[4];

	//
	// Get the number of pools.
	//
	result = g_DataSpaces->ReadVirtual(
		g_PoolState + g_HeapManagerStateOffsets.NumberOfPoolsOffset,
		&numberOfPools,
		sizeof(numberOfPools),
		nullptr);
	if (!SUCCEEDED(result))
	{
		goto Exit;
	}

	//
	// Iterate over all pool nodes, print all heaps (up to 4) in each
	//
	heapNumberToPoolType[0] = NonPagedPool;
	heapNumberToPoolType[1] = NonPagedPoolNx;
	heapNumberToPoolType[2] = PagedPool;
	heapNumberToPoolType[3] = PagedPool;

	//
	// Iterate over all pools and get the heaps from each.
	//
	for (ULONG j = 0; j < numberOfPools; j++)
	{
		//
		// Every POOL_NODE structure potentially contains 4 heaps.
		// First zero out our heaps array to not carry any information
		// over from a previous iteration.
		//
		for (int i = 0; i < 4; i++)
		{
			heaps[i] = 0;
		}

		//
		// The pool nodes are embedded inside PoolState.
		// Get to the offset of the current one we are looking at and read the addresses of the 4 heaps.
		//
		if (!SUCCEEDED(g_DataSpaces->ReadVirtual(
			g_PoolState + g_HeapManagerStateOffsets.PoolNodeOffset + (g_PoolNodeSize * j) + g_PoolNodeOffsets.HeapsOffset,
			&heaps,
			sizeof(heaps),
			nullptr)))
		{
			goto Exit;
		}
		//
		// Get basic information for each heap and parse it to
		// get information about the pool blocks it manages.
		//
		for (int i = 0; i < 4; i++)
		{
			//
			// This ugly if statement basically means:
			// If we requested only a specific pool type and this is the wrong pool type, skip this
			//
			if (((Flags.OnlyPaged) && (heapNumberToPoolType[i] != PagedPool)) ||
				(Flags.OnlyNonPaged) && (
					(heapNumberToPoolType[i] != NonPagedPool) && heapNumberToPoolType[i] != NonPagedPoolNx))
			{
				continue;
			}
			if (i == 0)
			{
				g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for NonPagedPool ***\n\n");
			}
			else if (i == 1)
			{
				g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for NonPagedPoolNx ***\n\n");
			}
			else if (i == 2)
			{
				g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for PagedPool ***\n\n");
			}
			else if (i == 3)
			{
				g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for Prototype PagedPool ***\n\n");
			}

			heap = {};
			GetInformationForHeap(heaps[i], &heap.Allocations, Tag);
			heap.Address = heaps[i];
			heap.NodeNumber = j;
			heap.Special = FALSE;
			heap.PoolType = heapNumberToPoolType[i];
			heap.NumberOfAllocations = heap.Allocations.size();
			g_Heaps.push_back(heap);
		}
	}
	//
	// Get special heaps from nt!ExPoolState->SpecialHeaps
	//
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual(
		g_PoolState + g_HeapManagerStateOffsets.SpecialHeapsOffset,
		&specialHeaps,
		sizeof(specialHeaps),
		nullptr)))
	{
		goto Exit;
	}
	for (int i = 0; i < 3; i++)
	{
		heap = {};
		if (i == 0)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for Special NonPagedPool ***\n\n");
		}
		else if (i == 1)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for Special NonPagedPoolNx ***\n\n");
		}
		if (i == 2)
		{
			g_DebugControl->Output(DEBUG_OUTPUT_DEBUGGEE, "*** Printing Data for Spacial PagedPool ***\n\n");
		}
		GetInformationForHeap(specialHeaps[i], &heap.Allocations, Tag);
		heap.Address = specialHeaps[i];
		heap.NodeNumber = -1;
		heap.Special = TRUE;
		heap.PoolType = heapNumberToPoolType[i];
		heap.NumberOfAllocations = heap.Allocations.size();
		g_Heaps.push_back(heap);
	}
Exit:
	return g_Heaps;
}

/*
	Receives an address in the pool and dumps information about it.
	Such as size, pool tag, type (LFH, VS), etc.
*/
VOID
GetPoolDataForAddress(
	_In_ PVOID Address
)
{
	PVOID heapPageSegment;
	PVOID heapPageSeg;
	ULONG pageIndex;
	ULONG result;
	ULONG unitShift;
	ULONG64 segmentMask;

	//
	// First check what type of allocation this is to know how to handle it
	//
	result = BitmapBitmaskRead(Address);

	//
	// Get the segment for the input address.
	// There is a HEAP_PAGE_SEGMENT for each MB so we need to align our address
	// to a MB to find it.
	//
	unitShift = 0;
	segmentMask = 0;
	if (result == 1)
	{
		//
		// SegContext[0]
		//
		segmentMask = 0xfffffffffff00000;
		unitShift = 0xC;
	}
	else if (result == 2)
	{
		//
		// SegContext[1]
		//
		segmentMask = 0xffffffffff000000;
		unitShift = 0x10;
	}
	else
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] ParseLargePoolAlloc\n", __FUNCTIONW__, __LINE__);
		//
		// LargePool
		//
		ParseLargePoolAlloc((ULONG64)Address, true, 0, 0);
		return;
	}




	//
	// Get the heap where the address is
	//
	heapPageSegment = (PVOID)((ULONG_PTR)(Address)&segmentMask);

	//
	// Find the index of our address' page in the descriptor's array.
	// There is a descriptor for every page, so we check how far our
	// page is from the segment state
	// (aligned to the nearest MB or 16 MB, depends on SegContext type).
	// There are 256 descriptors (0-0xFF) so we need to get the lowest byte
	// as the index.
	//
	pageIndex = ((((ULONG64)Address - (ULONG64)heapPageSegment) >> unitShift) & 0x00000000000ff);

	//
	// Allocate memory for the HEAP_PAGE_SEGMENT.
	//
	heapPageSeg = VirtualAlloc(NULL, g_HeapPageSegSize, MEM_COMMIT, PAGE_READWRITE);
	if (heapPageSeg == nullptr)
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed allocating memory\n", __FUNCTIONW__, __LINE__);
		goto Exit;
	}
	if (!SUCCEEDED(g_DataSpaces->ReadVirtual((ULONG64)heapPageSegment, heapPageSeg, g_HeapPageSegSize, nullptr)))
	{
		g_DebugControl->Output(DEBUG_OUTPUT_ERROR, "[%ws::%d] Failed reading memory: 0x%p\n", __FUNCTIONW__, __LINE__);
		goto Exit;
	}



	// g_HeapKey
	GetDataForDescriptor((ULONG64)heapPageSegment, heapPageSeg, pageIndex, unitShift, 0, Address, 0);
Exit:
	if (heapPageSeg != nullptr)
	{
		VirtualFree(heapPageSeg, NULL, MEM_RELEASE);
	}
}

/*
	Open a dmp file and initialize all the symbols and structures we will need to parse it.
*/
HRESULT
OpenDumpFile(
	_In_ PCSTR FilePath
)
{
	HRESULT result;
	result = g_DebugClient->OpenDumpFile(FilePath);
	if (!SUCCEEDED(result))
	{
		return result;
	}

	result = g_DebugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, 0);
	if (!SUCCEEDED(result))
	{
		return result;
	}

	if ((!SUCCEEDED(GetTypes())) ||
		(!SUCCEEDED(GetOffsets())) ||
		(!SUCCEEDED(GetSizes())) ||
		(!SUCCEEDED(GetHeapGlobals())))
	{
		return result;
	}

	return S_OK;
}

/*
	Enables or disables the chosen privilege for the current process.
	Taken from here: https://github.com/lilhoser/livedump
*/
BOOL
EnablePrivilege(
	_In_ PCWSTR PrivilegeName,
	_In_ BOOLEAN Acquire
)
{
	HANDLE tokenHandle;
	BOOL ret;
	ULONG tokenPrivilegesSize = FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges[1]);
	PTOKEN_PRIVILEGES tokenPrivileges = static_cast<PTOKEN_PRIVILEGES>(calloc(1, tokenPrivilegesSize));

	if (tokenPrivileges == NULL)
	{
		return FALSE;
	}

	tokenHandle = NULL;
	tokenPrivileges->PrivilegeCount = 1;
	ret = LookupPrivilegeValue(NULL,
		PrivilegeName,
		&tokenPrivileges->Privileges[0].Luid);
	if (ret == FALSE)
	{
		goto Exit;
	}

	tokenPrivileges->Privileges[0].Attributes = Acquire ? SE_PRIVILEGE_ENABLED
		: SE_PRIVILEGE_REMOVED;

	ret = OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES,
		&tokenHandle);
	if (ret == FALSE)
	{
		goto Exit;
	}

	ret = AdjustTokenPrivileges(tokenHandle,
		FALSE,
		tokenPrivileges,
		tokenPrivilegesSize,
		NULL,
		NULL);
	if (ret == FALSE)
	{
		goto Exit;
	}

Exit:
	if (tokenHandle != NULL)
	{
		CloseHandle(tokenHandle);
	}
	free(tokenPrivileges);
	return ret;
}

/*
	Creates a live dump of the current machine.
	Taken from here: https://github.com/lilhoser/livedump
*/
HRESULT
CreateDump(
	_In_ char* FilePath
)
{
	HRESULT result;
	HANDLE handle;
	HMODULE module;
	SYSDBG_LIVEDUMP_CONTROL_FLAGS flags;
	SYSDBG_LIVEDUMP_CONTROL_ADDPAGES pages;
	SYSDBG_LIVEDUMP_CONTROL liveDumpControl;
	NTSTATUS status;
	ULONG returnLength;

	handle = INVALID_HANDLE_VALUE;
	result = S_OK;
	flags.AsUlong = 0;
	pages.AsUlong = 0;

	//
	// Get function addresses
	//
	module = LoadLibrary(L"ntdll.dll");
	if (module == NULL)
	{
		result = S_FALSE;
		goto Exit;
	}

	g_NtSystemDebugControl = (NtSystemDebugControl)
		GetProcAddress(module, "NtSystemDebugControl");

	FreeLibrary(module);

	if (g_NtSystemDebugControl == NULL)
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Get SeDebugPrivilege
	//
	if (!EnablePrivilege(SE_DEBUG_NAME, TRUE))
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Create the target file (must specify synchronous I/O)
	//
	handle = CreateFileA(FilePath,
		GENERIC_WRITE | GENERIC_READ,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
		NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		result = S_FALSE;
		goto Exit;
	}

	//
	// Try to create the requested dump
	//
	memset(&liveDumpControl, 0, sizeof(liveDumpControl));

	//
	// The only thing the kernel looks at in the struct we pass is the handle,
	// the flags and the pages to dump.
	//
	liveDumpControl.DumpFileHandle = (PVOID)(handle);
	liveDumpControl.AddPagesControl = pages;
	liveDumpControl.Flags = flags;

	status = g_NtSystemDebugControl(CONTROL_KERNEL_DUMP,
		(PVOID)(&liveDumpControl),
		sizeof(liveDumpControl),
		NULL,
		0,
		&returnLength);

	if (NT_SUCCESS(status))
	{
		result = S_OK;
	}
	else
	{
		result = S_FALSE;
		goto Exit;
	}

Exit:
	if (handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(handle);
	}
	return result;
}

HRESULT
InitializeDebugGlobals(
	void
)
{
	HRESULT result;
	HMODULE handle;
	DebugCreateFunc debugCreate;

	handle = GetModuleHandle(L"DbgEng.dll");
	if (handle == 0)
	{
		result = S_FALSE;
		goto Exit;
	}
	debugCreate = (DebugCreateFunc)GetProcAddress(handle, "DebugCreate");

	result = debugCreate(__uuidof(IDebugClient), (PVOID*)&g_DebugClient);
	if (!SUCCEEDED(result))
	{
		printf("DebugCreate failed with error 0x%x\n", result);
		goto Exit;
	}

	result = g_DebugClient->QueryInterface(__uuidof(IDebugSymbols), (PVOID*)&g_DebugSymbols);
	if (!SUCCEEDED(result))
	{
		printf("QueryInterface for debug symbols failed with error 0x%x\n", result);
		goto Exit;
	}

	result = g_DebugClient->QueryInterface(__uuidof(IDebugDataSpaces), (PVOID*)&g_DataSpaces);
	if (!SUCCEEDED(result))
	{
		printf("QueryInterface for debug data spaces failed with error 0x%x\n", result);
		goto Exit;
	}

	result = g_DebugClient->QueryInterface(__uuidof(IDebugControl), (PVOID*)&g_DebugControl);
	if (!SUCCEEDED(result))
	{
		printf("QueryInterface for debug control failed with error 0x%x\n", result);
		goto Exit;
	}
Exit:
	return result;
}

VOID
UninitializeDebugGlobals(
	void
)
{
	if (g_DebugClient != nullptr)
	{
		g_DebugClient->Release();
	}
	if (g_DebugSymbols != nullptr)
	{
		g_DebugSymbols->Release();
	}
	if (g_DataSpaces != nullptr)
	{
		g_DataSpaces->Release();
	}
	if (g_DebugControl != nullptr)
	{
		g_DebugControl->Release();
	}
}

/*
	Open a dmp file and get information about all the heaps
	in it and the pool blocks they manage.
*/
HRESULT
GetPoolInformation(
	_In_ char* FilePath,
	_In_ bool CreateLiveDump,
	_Outptr_ int* NumberOfHeaps
)
{
	HRESULT result;
	POOL_VIEW_FLAGS flags;

	flags.AllFlags = 0;
	//
	// Initialize interfaces
	//

	if (CreateLiveDump)
	{
		if (!SUCCEEDED(CreateDump(FilePath)))
		{
			result = S_FALSE;
			goto Exit;
		}
	}

	result = InitializeDebugGlobals();

	//
	// Open dump file
	//

	result = OpenDumpFile(FilePath);
	if (!SUCCEEDED(result))
	{
		printf("OpenDumpFile failed with error 0x%x\n", result);
		goto Exit;
	}

	GetAllHeaps(NULL, flags);
	*NumberOfHeaps = g_Heaps.size();
	g_DebugClient->EndSession(DEBUG_END_ACTIVE_DETACH);

Exit:
	UninitializeDebugGlobals();
	if (CreateLiveDump)
	{
		DeleteFileA(FilePath);
	}
	return result;
}

/*
	Get all the information about one heap and remove it from g_Heaps.
	Save its allocations in g_CurrentAllocs so they can be queried.
*/
bool
GetNextHeapInformation(
	_Outptr_ ULONG64* Address,
	_Outptr_ int* NodeNumber,
	_Outptr_ long* NumberOfAllocations,
	_Outptr_ int* PoolType,
	_Outptr_ bool* Special
)
{
	if (g_Heaps.empty())
	{
		return 0;
	}
	std::list<HEAP>::iterator it = g_Heaps.begin();
	HEAP h = (*it);
	*Address = h.Address;
	*NodeNumber = h.NodeNumber;
	*NumberOfAllocations = h.NumberOfAllocations;
	*PoolType = h.PoolType;
	*Special = h.Special;
	g_CurrentAllocs = h.Allocations;
	g_Heaps.erase(it);
	return 1;
}

/*
	Get information about one pool block and remove it from g_CurrentAllocs.
*/
char*
GetNextAllocation(
	_Outptr_ ULONG64* Address,
	_Outptr_ int* Size,
	_Outptr_ bool* Allocated,
	_Outptr_ int* Type
)
{
	if (g_CurrentAllocs.empty())
	{
		*Size = 0;
		return 0;
	}
	std::list<ALLOC>::iterator it = g_CurrentAllocs.begin();
	ALLOC a = (*it);
	*Address = a.Address;
	*Size = a.Size;
	*Allocated = a.Allocated;
	*Type = (int)a.Type;
	g_CurrentAllocs.erase(it);

	char tag[5];
	tag[0] = a.PoolTag.c_str()[0];
	tag[1] = a.PoolTag.c_str()[1];
	tag[2] = a.PoolTag.c_str()[2];
	tag[3] = a.PoolTag.c_str()[3];
	tag[4] = '\x0';
	size_t stSize = strlen(tag) + sizeof(char);
	char* pszReturn = NULL;

	pszReturn = (char*)::CoTaskMemAlloc(stSize);
	strcpy_s(pszReturn, stSize, tag);
	return pszReturn;
}