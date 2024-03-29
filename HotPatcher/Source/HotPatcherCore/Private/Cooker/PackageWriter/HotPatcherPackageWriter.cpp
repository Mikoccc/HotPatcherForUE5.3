
#include "Cooker/PackageWriter/HotPatcherPackageWriter.h"

#if WITH_PACKAGE_CONTEXT && ENGINE_MAJOR_VERSION > 4
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/Async.h"
#include "Serialization/LargeMemoryWriter.h"

#include "UObject/SavePackage.h"

void FHotPatcherPackageWriter::Initialize(const FCookInfo& Info){}

// void FHotPatcherPackageWriter::AddToExportsSize(int64& ExportsSize)
// {
// 	TPackageWriterToSharedBuffer<ICookedPackageWriter>::AddToExportsSize(ExportsSize);
// }

void FHotPatcherPackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	TPackageWriterToSharedBuffer<ICookedPackageWriter>::BeginPackage(Info);
}

int64 FHotPatcherPackageWriter::GetExportsFooterSize()
{
	return sizeof(uint32);
}

void FHotPatcherPackageWriter::BeginCook(const FCookInfo& Info)
{
	
}

void FHotPatcherPackageWriter::EndCook(const FCookInfo& Info)
{
	
}

TFuture<FCbObject> FHotPatcherPackageWriter::WriteMPCookMessageForPackage(FName PackageName)
{
	return TFuture<FCbObject>();
}

bool FHotPatcherPackageWriter::TryReadMPCookMessageForPackage(FName PackageName, FCbObjectView Message)
{
	return false;
}

// void FHotPatcherPackageWriter::Flush()
// {
// 	UPackage::WaitForAsyncFileWrites();
// }

TUniquePtr<FAssetRegistryState> FHotPatcherPackageWriter::LoadPreviousAssetRegistry()
{
	return TUniquePtr<FAssetRegistryState>();
}

FCbObject FHotPatcherPackageWriter::GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey)
{
	return FCbObject();
}

void FHotPatcherPackageWriter::RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove)
{
}

void FHotPatcherPackageWriter::RemoveCookedPackages()
{
	UPackage::WaitForAsyncFileWrites();
}

void FHotPatcherPackageWriter::MarkPackagesUpToDate(TArrayView<const FName> UpToDatePackages)
{
}

bool FHotPatcherPackageWriter::GetPreviousCookedBytes(const FPackageInfo& Info, FPreviousCookedBytesData& OutData)
{
	return ICookedPackageWriter::GetPreviousCookedBytes(Info, OutData);
}

PRAGMA_DISABLE_OPTIMIZATION

void FHotPatcherPackageWriter::CompleteExportsArchiveForDiff(FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive)
{
	FPackageWriterRecords::FPackage& BaseRecord = Records.FindRecordChecked(Info.PackageName);
	FRecord& Record = static_cast<FRecord&>(BaseRecord);
	Record.bCompletedExportsArchiveForDiff = true;

	// Add on all the attachments which are usually added on during Commit. The order must match AsyncSave.
	for (FBulkDataRecord& BulkRecord : Record.BulkDatas)
	{
		if (BulkRecord.Info.BulkDataType == FBulkDataInfo::AppendToExports && BulkRecord.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(BulkRecord.Buffer.GetData()),
				BulkRecord.Buffer.GetSize());
		}
	}
	for (FLinkerAdditionalDataRecord& AdditionalRecord : Record.LinkerAdditionalDatas)
	{
		if (AdditionalRecord.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(AdditionalRecord.Buffer.GetData()),
				AdditionalRecord.Buffer.GetSize());
		}
	}

	uint32 FooterData = PACKAGE_FILE_TAG;
	ExportsArchive << FooterData;

	for (FPackageTrailerRecord& PackageTrailer : Record.PackageTrailers)
	{
		if (PackageTrailer.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(PackageTrailer.Buffer.GetData()),
				PackageTrailer.Buffer.GetSize());
		}
	}
}

EPackageWriterResult FHotPatcherPackageWriter::BeginCacheForCookedPlatformData(
	FBeginCacheForCookedPlatformDataInfo& Info)
{
	return EPackageWriterResult::Success;
}

PRAGMA_ENABLE_OPTIMIZATION

void FHotPatcherPackageWriter::CollectForSavePackageData(FRecord& Record, FCommitContext& Context)
{
	Context.ExportsBuffers.AddDefaulted(Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Package.Buffer, MoveTemp(Package.Regions) });
	}
}

void FHotPatcherPackageWriter::CollectForSaveBulkData(FRecord& Record, FCommitContext& Context)
{
	for (FBulkDataRecord& BulkRecord : Record.BulkDatas)
	{
		if (BulkRecord.Info.BulkDataType == FBulkDataInfo::AppendToExports)
		{
			if (Record.bCompletedExportsArchiveForDiff)
			{
				// Already Added in CompleteExportsArchiveForDiff
				continue;
			}
			Context.ExportsBuffers[BulkRecord.Info.MultiOutputIndex].Add(FExportBuffer{ BulkRecord.Buffer, MoveTemp(BulkRecord.Regions) });
		}
		else
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = BulkRecord.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(BulkRecord.Buffer);
			OutputFile.Regions = MoveTemp(BulkRecord.Regions);
			OutputFile.bIsSidecar = true;
			OutputFile.bContributeToHash = BulkRecord.Info.MultiOutputIndex == 0; // Only caculate the main package output hash
		}
	}
}

void FHotPatcherPackageWriter::CollectForSaveLinkerAdditionalDataRecords(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FLinkerAdditionalDataRecord& AdditionalRecord : Record.LinkerAdditionalDatas)
	{
		Context.ExportsBuffers[AdditionalRecord.Info.MultiOutputIndex].Add(FExportBuffer{ AdditionalRecord.Buffer, MoveTemp(AdditionalRecord.Regions) });
	}
}

void FHotPatcherPackageWriter::CollectForSaveAdditionalFileRecords(FRecord& Record, FCommitContext& Context)
{
	for (FAdditionalFileRecord& AdditionalRecord : Record.AdditionalFiles)
	{
		FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
		OutputFile.Filename = AdditionalRecord.Info.Filename;
		OutputFile.Buffer = FCompositeBuffer(AdditionalRecord.Buffer);
		OutputFile.bIsSidecar = true;
		OutputFile.bContributeToHash = AdditionalRecord.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
	}
}

void FHotPatcherPackageWriter::CollectForSaveExportsFooter(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	uint32 FooterData = PACKAGE_FILE_TAG;
	FSharedBuffer Buffer = FSharedBuffer::Clone(&FooterData, sizeof(FooterData));
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Buffer, TArray<FFileRegion>() });
	}
}

void FHotPatcherPackageWriter::CollectForSaveExportsPackageTrailer(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FPackageTrailerRecord& PackageTrailer : Record.PackageTrailers)
	{
		Context.ExportsBuffers[PackageTrailer.Info.MultiOutputIndex].Add(
			FExportBuffer{ PackageTrailer.Buffer, TArray<FFileRegion>() });
	}
}

void FHotPatcherPackageWriter::CollectForSaveExportsBuffers(FRecord& Record, FCommitContext& Context)
{
	check(Context.ExportsBuffers.Num() == Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		TArray<FExportBuffer>& ExportsBuffers = Context.ExportsBuffers[Package.Info.MultiOutputIndex];
		check(ExportsBuffers.Num() > 0);

		// Split the ExportsBuffer into (1) Header and (2) Exports + AllAppendedData
		int64 HeaderSize = Package.Info.HeaderSize;
		FExportBuffer& HeaderAndExportsBuffer = ExportsBuffers[0];
		FSharedBuffer& HeaderAndExportsData = HeaderAndExportsBuffer.Buffer;

		// Header (.uasset/.umap)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = Package.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(
				FSharedBuffer::MakeView(HeaderAndExportsData.GetData(), HeaderSize, HeaderAndExportsData));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
		}

		// Exports + AllAppendedData (.uexp)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = FPaths::ChangeExtension(Package.Info.LooseFilePath, LexToString(EPackageExtension::Exports));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only caculate the main package output hash

			int32 NumBuffers = ExportsBuffers.Num();
			TArray<FSharedBuffer> BuffersForComposition;
			BuffersForComposition.Reserve(NumBuffers);

			const uint8* ExportsStart = static_cast<const uint8*>(HeaderAndExportsData.GetData()) + HeaderSize;
			BuffersForComposition.Add(FSharedBuffer::MakeView(ExportsStart, HeaderAndExportsData.GetSize() - HeaderSize,
				HeaderAndExportsData));
			OutputFile.Regions.Append(MoveTemp(HeaderAndExportsBuffer.Regions));

			for (FExportBuffer& ExportsBuffer : TArrayView<FExportBuffer>(ExportsBuffers).Slice(1, NumBuffers - 1))
			{
				BuffersForComposition.Add(ExportsBuffer.Buffer);
				OutputFile.Regions.Append(MoveTemp(ExportsBuffer.Regions));
			}
			OutputFile.Buffer = FCompositeBuffer(BuffersForComposition);

			// Adjust regions so they are relative to the start of the uexp file
			for (FFileRegion& Region : OutputFile.Regions)
			{
				Region.Offset -= HeaderSize;
			}
		}
	}
}
PRAGMA_DISABLE_OPTIMIZATION


TFuture<FMD5Hash> FHotPatcherPackageWriter::AsyncSave(FRecord& Record, const FCommitPackageInfo& Info)
{
	FCommitContext Context{ Info };

	// The order of these collection calls is important, both for ExportsBuffers (affects the meaning of offsets
	// to those buffers) and for OutputFiles (affects the calculation of the Hash for the set of PackageData)
	// The order of ExportsBuffers must match CompleteExportsArchiveForDiff.
	CollectForSavePackageData(Record, Context);
	CollectForSaveBulkData(Record, Context);
	CollectForSaveLinkerAdditionalDataRecords(Record, Context);
	CollectForSaveAdditionalFileRecords(Record, Context);
	CollectForSaveExportsFooter(Record, Context);
	CollectForSaveExportsPackageTrailer(Record, Context);
	CollectForSaveExportsBuffers(Record, Context);

	return AsyncSaveOutputFiles(Record, Context);
}
PRAGMA_ENABLE_OPTIMIZATION
TFuture<FMD5Hash> FHotPatcherPackageWriter::AsyncSaveOutputFiles(FRecord& Record, FCommitContext& Context)
{
	if (!EnumHasAnyFlags(Context.Info.WriteOptions, EWriteOptions::Write | EWriteOptions::ComputeHash))
	{
		return TFuture<FMD5Hash>();
	}

	UE::SavePackageUtilities::IncrementOutstandingAsyncWrites();
	FMD5Hash OutputHash;
	FMD5 AccumulatedHash;
	for (FWriteFileData& OutputFile : Context.OutputFiles)
	{
		OutputFile.Write(AccumulatedHash, Context.Info.WriteOptions);
	}
	OutputHash.Set(AccumulatedHash);
	
	return Async(EAsyncExecution::TaskGraph,[OutputHash]()mutable ->FMD5Hash
	{
		UE::SavePackageUtilities::DecrementOutstandingAsyncWrites();
		return OutputHash;
	});
}

FDateTime FHotPatcherPackageWriter::GetPreviousCookTime() const
{
	FString MetadataDirectoryPath = FPaths::ProjectDir() / TEXT("Metadata");
	const FString PreviousAssetRegistry = FPaths::Combine(MetadataDirectoryPath, GetDevelopmentAssetRegistryFilename());
	return IFileManager::Get().GetTimeStamp(*PreviousAssetRegistry);
}

void FHotPatcherPackageWriter::CommitPackageInternal(FPackageRecord&& Record,
	const IPackageWriter::FCommitPackageInfo& Info)
{
	FRecord& InRecord = static_cast<FRecord&>(Record);
	TFuture<FMD5Hash> CookedHash;
	if (Info.Status == ECommitStatus::Success)
	{
		CookedHash = AsyncSave(InRecord, Info);
	}
}

FPackageWriterRecords::FPackage* FHotPatcherPackageWriter::ConstructRecord()
{
	return new FRecord();
}

static void WriteToFile(const FString& Filename, const FCompositeBuffer& Buffer)
{
	IFileManager& FileManager = IFileManager::Get();

	struct FFailureReason
	{
		uint32 LastErrorCode = 0;
		bool bSizeMatchFailed = false;
	};
	TOptional<FFailureReason> FailureReason;

	for (int32 Tries = 0; Tries < 3; ++Tries)
	{
		FArchive* Ar = FileManager.CreateFileWriter(*Filename);
		if (!Ar)
		{
			if (!FailureReason)
			{
				FailureReason = FFailureReason{ FPlatformMisc::GetLastError(), false };
			}
			continue;
		}

		int64 DataSize = 0;
		for (const FSharedBuffer& Segment : Buffer.GetSegments())
		{
			int64 SegmentSize = static_cast<int64>(Segment.GetSize());
			Ar->Serialize(const_cast<void*>(Segment.GetData()), SegmentSize);
			DataSize += SegmentSize;
		}
		delete Ar;

		if (FileManager.FileSize(*Filename) != DataSize)
		{
			if (!FailureReason)
			{
				FailureReason = FFailureReason{ 0, true };
			}
			FileManager.Delete(*Filename);
			continue;
		}
		return;
	}

	TCHAR LastErrorText[1024];
	if (FailureReason && FailureReason->bSizeMatchFailed)
	{
		FCString::Strcpy(LastErrorText, TEXT("Unexpected file size. Another operation is modifying the file, or the write operation failed to write completely."));
	}
	else if (FailureReason && FailureReason->LastErrorCode != 0)
	{
		FPlatformMisc::GetSystemErrorMessage(LastErrorText, UE_ARRAY_COUNT(LastErrorText), FailureReason->LastErrorCode);
	}
	else
	{
		FCString::Strcpy(LastErrorText, TEXT("Unknown failure reason."));
	}
	UE_LOG(LogTemp, Fatal, TEXT("SavePackage Async write %s failed: %s"), *Filename, LastErrorText);
}

void FHotPatcherPackageWriter::FWriteFileData::Write(FMD5& AccumulatedHash, EWriteOptions WriteOptions) const
{
	//@todo: FH: Should we calculate the hash of both output, currently only the main package output hash is calculated
	if (EnumHasAnyFlags(WriteOptions, EWriteOptions::ComputeHash) && bContributeToHash)
	{
		for (const FSharedBuffer& Segment : Buffer.GetSegments())
		{
			AccumulatedHash.Update(static_cast<const uint8*>(Segment.GetData()), Segment.GetSize());
		}
	}

	if ((bIsSidecar && EnumHasAnyFlags(WriteOptions, EWriteOptions::WriteSidecars)) ||
		(!bIsSidecar && EnumHasAnyFlags(WriteOptions, EWriteOptions::WritePackage)))
	{
		const FString* WriteFilename = &Filename;
		FString FilenameBuffer;
		if (EnumHasAnyFlags(WriteOptions, EWriteOptions::SaveForDiff))
		{
			FilenameBuffer = FPaths::Combine(FPaths::GetPath(Filename),
				FPaths::GetBaseFilename(Filename) + TEXT("_ForDiff") + FPaths::GetExtension(Filename, true));
			WriteFilename = &FilenameBuffer;
		}
		WriteToFile(*WriteFilename, Buffer);

		if (Regions.Num() > 0)
		{
			TArray<uint8> Memory;
			FMemoryWriter Ar(Memory);
			FFileRegion::SerializeFileRegions(Ar, const_cast<TArray<FFileRegion>&>(Regions));

			WriteToFile(*WriteFilename + FFileRegion::RegionsFileExtension,
				FCompositeBuffer(FSharedBuffer::MakeView(Memory.GetData(), Memory.Num())));
		}
	}
}

#endif