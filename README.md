# HotPatcherForUE5.3
# 不保证有BUG！不保证有BUG！不保证有BUG！

只是为了能够在UE5.3使用上，目前已知的BUG HotPatcher的Cook存在问题，建议不要使用（虽然现在真的使用也能跑）。

因为有其他活了，出问题了再回来更新。

# 修改记录：

相关头文件增加前缀:AssetRegistry/



“/std:c++20”中已弃用通过 "[=]" 来隐式捕获 "this"



两个枚举的替换：

EAssetRegistryDependencyType::Type

UE::AssetRegistry::EDependencyCategory

```
static_cast<UE::AssetRegistry::EDependencyCategory>((uint8)AssetDepType)
```



LoadPackageAsync()

参数重载缺少



LoadShaderPipelineCache中使用ShaderPlatformToShaderFormatName 

这个函数5.3没了 找到了替代函数

ShaderPlatformToPlatformName()



接口缺失FHotPatcherPackageWriter



HotPatcherPackageWriter

缺少头文件 导致报错error C4150: deletion of pointer to incomplete type 'XXXXX'; no destructor called



```
std::ignore = ProgressPtr.Release();
```

C4834在C++20中[[nodiscard]]会报错 需要忽略



IWorldPartitionCookPackageContext

增加接口GatherPackagesToCook

FWorldPartitionCookPackageContext这个类（在插件里有一份，UE源码的是private的）是UE源码里面复制过来的 增加了新接口需要重新抄袭一下。

HotWorldPartitionCookPackageSplitter也同理有更改

HotPather的Cook自定义的。



（接口需要注意 不知道作用）

FHotPatcherPackageWriter接口报错

ICookedPackageWriter接口参数更改

AddToExportsSize 接口删除

WriteMPCookMessageForPackage接口增加

TryReadMPCookMessageForPackage接口增加



GEditor->Save()

去除废弃函数将函数参数报给了FSavePackageArgs

FlibHotPatcherCoreHelper 649行相关修改



UAssetManager::VerifyCanCookPackage去除废弃函数

bool UFlibHotPatcherCoreHelper::IsCanCookPackage

相关修改



GEditor->OnPreSaveWorld去除废弃函数

void UFlibHotPatcherCoreHelper::CacheForCookedPlatformData

2183行修改 将saveflags包在结构体FObjectPreSaveContext中

