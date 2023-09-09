# Unreal Engine Asset Toolkit-Fixes
A set of 2 plugins, one for generating asset dumps from the game and second for making assets from them in editor. This version of the tool is geared more towards asset generation using the UEAssetToolkitGenerator tool (AKA CAS). Even though generating with this tool is slightly easier than with the main one, it is still **VERY** difficult, and will likely require code changes. Please see the code changes in the tips section below for tips on where to look.

## To Install
These are plugins, are both are required to install. All you need to do, is place these two folders into the project's plugins folders. If they don't show up in your project correctly, delete all of the intermediate/saved and .sln file and regenerate visual studio solution files from the .uproject. 

## Issues/help
For issues compiling the project with [UnrealPSKPSA](https://github.com/djhaled/UnrealPSKPSA): the current version of UnrealPSKPSA is only backported to 4.27. If you are on an earlier version (i.e. 4.25 or 4.26), and know what you are doing, please do try to backport to those versions and make a pull request. Otherwise, you can go back to the `main` branch which is for commits without the UnrealPSKPSA plugin [here](https://github.com/Buckminsterfullerene02/UEAssetToolkit-Fixes/).

You can ask for help in the [UE Modding Discord](https://discord.gg/zVvsE9mEEa) in the #questions forum channel. Please make sure to add [UEAT] into the post title.

## Creating Asset Dumps
Use CAS to create the JSON dumps.

## Generating In-editor
To import assets into the editor, navigate to Window -> Developer Tools -> Asset Generator.
Specify directory with the asset dump in the settings, tweak them as necessary, and press Generate Assets.
It will also automatically refresh existing assets and create dependencies for the assets you have selected, even if you haven't checked them for generation.

## Generating using CLI
The commandlet name is `AssetGenerator`. These are the following flags you may use:
```
-DumpDirectory= is the path to the root directory of the dump, as specified in the asset dumper

-ForceGeneratePackageNames= is optional file contaning a newline-separated list of packages to be generated first

-BlacklistPackageNames= is optional, semantics are the same as for ForceGeneratePackageNames, except that it also supports wildcard paths if they end with /, not really needed by default

-AssetClassWhitelist= is optional comma-delimited list of whitelisted asset classes to generate, should be left empty for full project generation

-PublicProject is optional and nulls out non-distributable assets in the generated project, if not specified it will generate a full project containing models and textures as they are in the game (developed for Satisfactory originally, and this is something the developers asked for)

-NoRefresh is optional and prevents the generator from touching existing assets if specified
```

Example command line:
```
"F:\UNEPIC GAMES\UE_4.27\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "F:\DRG Modding\Project Generator\FSDTemplateU37P8-all\FSD.uproject" -run=AssetGenerator -DumpDirectory="F:\DRG Modding\DRGPacker\JSON\Assets" -AssetClassWhitelist="Texture2D,Material" -NoRefresh -abslog="F:\DRG Modding\DRGPacker\JSON\AssetGen\AssetGenerator.log" -stdout -unattended -NoLogTimes
```

Assets that you MUST generate in-editor:
- SoundCue
- Assets using FBX/ActorX file e.g. Static Mesh, Skeletal Mesh, AnimSequence

## Tips
- Sometimes when generating, some assets or asset types cause consistent crashes due to missing data so we may want to filter them out. If any specific asset type/package/directory is giving you a massive headache and you want to skip it, edit `IsDumbAsset()` in `AssetTypeGenerator.cpp` line `74`. This is a temporary solution per generation/project.
- When you are generating in-editor, you almost certainly almost want to be running in debug mode so that your IDE will show a full stack breakpoint on the line that the exception occurs, so you can see the values of each set of properties in each stack. It is ideal to even download the engine editor symbols, which are actually 10x smaller than the size of EGS store says (silly bug). So for 4.27 this was 3.5GBs.
- The main "logic" occurs in `UAssetTypeGenerator::AdvanceGenerationState()`, so you will probably be stepping through this in debug mode to see exactly what is going on, if you are crashing for an unexplainable reason.
- If you are getting crashes with blueprint interfaces, it is almost certainly during the `UBlueprintGenerator::PostConstructOrUpdateAsset` stage, so temporarily comment out the lines `97`-`100` for that asset(s).
- Be careful about the `RowStruct` property in the serialized JSON for `DataTable` assets. This can quite easily be trying to reference one that doesn't exist (as it is nativized or something), and will cause really annoying crashes as they can take ages to diagnose.
- For natitivzed assets, CAS should serialize BPs/enums if they fall in the right settings, but **NOT** structs. So you will have to dummy these manually prior.