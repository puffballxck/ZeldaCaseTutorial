#include "ZCBokoblinSetupCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "ActorFactories/ActorFactory.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "BlueprintEditorLibrary.h"
#include "Builders/CubeBuilder.h"
#include "Components/BrushComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	const FString EnemyFolder = TEXT("/Game/_Game/Animations/Enemy/Bokoblin/");
	const FString AIFolder = TEXT("/Game/_Game/AI/Bokoblin/");

	template<class T> T* LoadRequired(const FString& Path)
	{
		T* Result = LoadObject<T>(nullptr, *Path);
		checkf(Result, TEXT("Missing required asset: %s"), *Path);
		return Result;
	}

	UClass* RuntimeClass(const TCHAR* Name)
	{
		UClass* Result = LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/ZCase.%s"), Name));
		checkf(Result, TEXT("Missing runtime class: %s"), Name);
		return Result;
	}

	void Set(UObject* Object, const FName Name, const FString& Value)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(Name);
		checkf(Property, TEXT("Missing %s.%s"), *Object->GetName(), *Name.ToString());
		Object->Modify();
		checkf(Property->ImportText_Direct(*Value, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None),
			TEXT("Cannot set %s.%s = %s"), *Object->GetName(), *Name.ToString(), *Value);
	}

	void Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		checkf(UPackage::SavePackage(Package, Asset, *Filename, Args), TEXT("Save failed: %s"), *Filename);
		UE_LOG(LogTemp, Display, TEXT("Bokoblin setup saved %s"), *Filename);
	}

	template<class T> T* NewAsset(const FString& Path)
	{
		UPackage* Package = CreatePackage(*Path);
		T* Asset = NewObject<T>(Package, *FPackageName::GetLongPackageAssetName(Path), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	UBehaviorTree* BuildTree()
	{
		if (UBehaviorTree* Existing = LoadObject<UBehaviorTree>(nullptr, *(AIFolder + TEXT("BT_Bokoblin"))))
		{
			return Existing; // Preserve subsequent hand edits when setup is run again.
		}
		UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *(AIFolder + TEXT("BB_Bokoblin")));
		if (!BB) BB = NewAsset<UBlackboardData>(AIFolder + TEXT("BB_Bokoblin"));
		BB->Keys.Reset();
		FBlackboardEntry Target;
		Target.EntryName = TEXT("TargetActor");
		auto* ActorKey = NewObject<UBlackboardKeyType_Object>(BB);
		ActorKey->BaseClass = AActor::StaticClass();
		Target.KeyType = ActorKey;
		BB->Keys.Add(Target);
		FBlackboardEntry Patrol;
		Patrol.EntryName = TEXT("PatrolLocation");
		Patrol.KeyType = NewObject<UBlackboardKeyType_Vector>(BB);
		BB->Keys.Add(Patrol);
		Save(BB);

		UBehaviorTree* Tree = NewAsset<UBehaviorTree>(AIFolder + TEXT("BT_Bokoblin"));
		Tree->BlackboardAsset = BB;
		auto* Root = NewObject<UBTComposite_Selector>(Tree);
		Tree->RootNode = Root;
		for (int32 Branch = 0; Branch != 2; ++Branch)
		{
			auto* Sequence = NewObject<UBTComposite_Sequence>(Tree);
			Sequence->NodeName = Branch == 0 ? TEXT("Combat") : TEXT("Patrol");
			FBTCompositeChild Child;
			Child.ChildComposite = Sequence;
			auto* Decorator = NewObject<UBTDecorator_Blackboard>(Tree);
			Set(Decorator, TEXT("BlackboardKey"), TEXT("(SelectedKeyName=TargetActor)"));
			Set(Decorator, TEXT("FlowAbortMode"), TEXT("Both"));
			Set(Decorator, TEXT("NotifyObserver"), TEXT("ValueChange"));
			Set(Decorator, TEXT("OperationType"), FString::FromInt(Branch == 0 ? EBasicKeyOperation::Set : EBasicKeyOperation::NotSet));
			Set(Decorator, TEXT("BasicOperation"), Branch == 0 ? TEXT("Set") : TEXT("NotSet"));
			Child.Decorators.Add(Decorator);
			Root->Children.Add(Child);
			auto AddTask = [Tree, Sequence](UClass* Class)
			{
				auto* Task = NewObject<UBTTaskNode>(Tree, Class);
				FBTCompositeChild TaskChild;
				TaskChild.ChildTask = Task;
				Sequence->Children.Add(TaskChild);
				return Task;
			};
			if (Branch == 0)
			{
				AddTask(RuntimeClass(TEXT("ZCBTTask_Chase")));
				AddTask(RuntimeClass(TEXT("ZCBTTask_Attack")));
			}
			else
			{
				AddTask(RuntimeClass(TEXT("ZCBTTask_FindPatrolPoint")));
				auto* Move = CastChecked<UBTTask_MoveTo>(AddTask(UBTTask_MoveTo::StaticClass()));
				Set(Move, TEXT("BlackboardKey"), TEXT("(SelectedKeyName=PatrolLocation)"));
				Move->AcceptableRadius = 45.0f;
				Move->bAllowPartialPath = false;
			}
			auto* Wait = CastChecked<UBTTask_Wait>(AddTask(UBTTask_Wait::StaticClass()));
			Wait->WaitTime = Branch == 0 ? 0.6f : 2.25f;
			Wait->RandomDeviation = Branch == 0 ? 0.2f : 0.75f;
		}
		Tree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(Tree, TEXT("Behavior Tree"),
			UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass());
		auto* Graph = CastChecked<UBehaviorTreeGraph>(Tree->BTGraph);
		Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
		Graph->OnCreated();
		Graph->Initialize();
		// AutoArrange requires Slate node widgets, which a commandlet does not own.
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			auto* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
			if (!BTNode) continue;
			if (BTNode->NodeInstance == Root) { Node->NodePosX = 700; Node->NodePosY = 150; }
			for (int32 Branch = 0; Branch < Root->Children.Num(); ++Branch)
			{
				auto* Sequence = Root->Children[Branch].ChildComposite.Get();
				const int32 BranchX = Branch * 1200;
				if (BTNode->NodeInstance == Sequence) { Node->NodePosX = BranchX + 400; Node->NodePosY = 350; }
				for (int32 TaskIndex = 0; TaskIndex < Sequence->Children.Num(); ++TaskIndex)
					if (BTNode->NodeInstance == Sequence->Children[TaskIndex].ChildTask)
					{ Node->NodePosX = BranchX + TaskIndex * 360; Node->NodePosY = 650; }
			}
		}
		Graph->UpdateAsset();
		// UpdateAsset rebuilds the runtime tree from graph nodes. Recreate the two
		// branch decorators afterward so their serialized references are owned by
		// the BehaviorTree package even in commandlet mode.
		for (int32 Branch = 0; Branch < Root->Children.Num(); ++Branch)
		{
			auto* Decorator = NewObject<UBTDecorator_Blackboard>(Tree,
			Branch == 0 ? TEXT("TargetSetDecorator") : TEXT("TargetNotSetDecorator"), RF_Transactional);
			Set(Decorator, TEXT("BlackboardKey"), TEXT("(SelectedKeyName=TargetActor)"));
			Set(Decorator, TEXT("FlowAbortMode"), TEXT("Both"));
			Set(Decorator, TEXT("NotifyObserver"), TEXT("ValueChange"));
			Set(Decorator, TEXT("OperationType"), FString::FromInt(Branch == 0 ? EBasicKeyOperation::Set : EBasicKeyOperation::NotSet));
			Set(Decorator, TEXT("BasicOperation"), Branch == 0 ? TEXT("Set") : TEXT("NotSet"));
			Root->Children[Branch].Decorators.Reset();
			Root->Children[Branch].Decorators.Add(Decorator);
			Root->Children[Branch].DecoratorOps.Reset();
		}
		check(Tree->RootNode && Tree->RootNode->Children.Num() == 2);
		Save(Tree);
		return Tree;
	}

	UAnimMontage* BuildAttack()
	{
		const FString Path = EnemyFolder + TEXT("Animation/Montage/AM_Bokoblin_Attack_01");
		if (auto* Existing = LoadObject<UAnimMontage>(nullptr, *Path)) return Existing;
		auto* Sequence = LoadRequired<UAnimSequence>(EnemyFolder + TEXT("Animation/00_Combat/01_Attack/Unarmed/Enemy_Bokoblin-AniArmature_Attack_Punch_R"));
		auto* Montage = NewAsset<UAnimMontage>(Path);
		Montage->SetSkeleton(Sequence->GetSkeleton());
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequence, true);
		Montage->SlotAnimTracks[0].SlotName = TEXT("DefaultSlot");
		Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Add(Segment);
		Montage->SetCompositeLength(Sequence->GetPlayLength());
		static_cast<UAnimCompositeBase*>(Montage)->UpdateCommonTargetFrameRate();
		Montage->AddAnimCompositeSection(TEXT("Default"), 0.0f);
		Montage->BlendIn.SetBlendTime(0.1f);
		Montage->BlendOut.SetBlendTime(0.15f);
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, TEXT("Damage"));
		// Raw right-wrist sampling puts the forward strike at 0.80-0.92 s in this 1.76 s clip.
		// Kept on the montage so artists can tune the contact frames directly.
		const float Start = 0.80f;
		const float Duration = 0.12f;
		check(UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(Montage, TEXT("Damage"), Start, Duration,
			RuntimeClass(TEXT("ZCAnimNotifyState_WeaponTrace"))));
		Save(Montage);
		return Montage;
	}

	void InspectPunch()
	{
		auto* Sequence = LoadRequired<UAnimSequence>(EnemyFolder + TEXT("Animation/00_Combat/01_Attack/Unarmed/Enemy_Bokoblin-AniArmature_Attack_Punch_R"));
		const FReferenceSkeleton& Skeleton = Sequence->GetSkeleton()->GetReferenceSkeleton();
		UE_LOG(LogTemp, Display, TEXT("Punch length %.3f, rate %.3f"), Sequence->GetPlayLength(), Sequence->RateScale);
		for (float Time = 0.0f; Time <= Sequence->GetPlayLength(); Time += 0.04f)
		{
			FTransform Hand = FTransform::Identity;
			for (int32 Bone = Skeleton.FindBoneIndex(TEXT("Wrist_R")); Bone != INDEX_NONE; Bone = Skeleton.GetParentIndex(Bone))
			{
				FTransform Local;
				Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(static_cast<double>(Time)), true);
				Hand *= Local;
			}
			const FVector ActorSpace = FRotator(0, -90, 0).RotateVector(Hand.GetLocation() * 0.75f) + FVector(0, 0, -81.8444f);
			UE_LOG(LogTemp, Display, TEXT("Punch sample %.2f: %s"), Time, *ActorSpace.ToString());
		}
	}

	void BuildLocomotion()
	{
		const FString Path = EnemyFolder + TEXT("BS_Bokoblin_Locomotion");
		auto* Blend = LoadObject<UBlendSpace1D>(nullptr, *Path);
		if (!Blend)
		{
			Blend = NewAsset<UBlendSpace1D>(Path);
			auto* Idle = LoadRequired<UAnimSequence>(EnemyFolder + TEXT("Animation/40_Idle_Life/Idle/Enemy_Bokoblin-AniArmature_Wait"));
			Blend->SetSkeleton(Idle->GetSkeleton());
			auto* Axis = FindFProperty<FStructProperty>(Blend->GetClass(), TEXT("BlendParameters"));
			check(Axis);
			auto* Parameter = Axis->ContainerPtrToValuePtr<FBlendParameter>(Blend);
			Parameter->DisplayName = TEXT("Speed");
			Parameter->Min = 0.0f;
			Parameter->Max = 400.0f;
			Parameter->bSnapToGrid = false;
			Blend->AddSample(Idle, FVector::ZeroVector);
			Blend->AddSample(LoadRequired<UAnimSequence>(EnemyFolder + TEXT("Animation/10_Movement/Basic_Locomotion/Enemy_Bokoblin-AniArmature_Walk")), FVector(180, 0, 0));
			Blend->AddSample(LoadRequired<UAnimSequence>(EnemyFolder + TEXT("Animation/10_Movement/Basic_Locomotion/Enemy_Bokoblin-AniArmature_Run")), FVector(400, 0, 0));
			Blend->ValidateSampleData();
			Blend->ResampleData();
			Save(Blend);
		}
		auto* BP = LoadRequired<UAnimBlueprint>(EnemyFolder + TEXT("ABP_Enemy_Bokoblin"));
		if (BP->ParentClass != RuntimeClass(TEXT("ZCBokoblinAnimInstance")))
			UBlueprintEditorLibrary::ReparentBlueprint(BP, RuntimeClass(TEXT("ZCBokoblinAnimInstance")));
		UEdGraph* Graph = nullptr;
		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (UEdGraph* Candidate : Graphs)
			if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate;
		check(Graph);
		UAnimGraphNode_SequencePlayer* OldIdle = nullptr;
		UAnimGraphNode_Slot* Slot = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (auto* Player = Cast<UAnimGraphNode_SequencePlayer>(Node)) OldIdle = Player;
			if (auto* FoundSlot = Cast<UAnimGraphNode_Slot>(Node)) Slot = FoundSlot;
		}
		check(Slot && Slot->Node.SlotName == TEXT("DefaultSlot"));
		if (OldIdle)
		{
			FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> PlayerCreator(*Graph);
			auto* Player = PlayerCreator.CreateNode();
			Player->Node.SetBlendSpace(Blend);
			Player->NodePosX = -700;
			PlayerCreator.Finalize();
			FGraphNodeCreator<UK2Node_VariableGet> SpeedCreator(*Graph);
			auto* Speed = SpeedCreator.CreateNode();
			Speed->VariableReference.SetSelfMember(TEXT("Speed"));
			Speed->NodePosX = -1000;
			SpeedCreator.Finalize();
			const UEdGraphSchema* Schema = Graph->GetSchema();
			check(Schema->TryCreateConnection(Speed->FindPinChecked(TEXT("Speed")), Player->FindPinChecked(TEXT("X"))));
			Slot->FindPinChecked(TEXT("Source"))->BreakAllPinLinks();
			check(Schema->TryCreateConnection(Player->FindPinChecked(TEXT("Pose")), Slot->FindPinChecked(TEXT("Source"))));
			FBlueprintEditorUtils::RemoveNode(BP, OldIdle, true);
			Graph->NotifyGraphChanged();
		}
		FKismetEditorUtilities::CompileBlueprint(BP);
		check(BP->Status != BS_Error);
		Save(BP);
	}
}

UZCBokoblinSetupCommandlet::UZCBokoblinSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UZCBokoblinSetupCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params, TEXT("Inspect"))) { InspectPunch(); return 0; }
	UBehaviorTree* Tree = BuildTree();
	UAnimMontage* Attack = BuildAttack();
	BuildLocomotion();
	auto* BP = LoadRequired<UBlueprint>(TEXT("/Game/_Game/Blueprints/Enmies/BP_Enemy_Bokoblin"));
	if (BP->ParentClass != RuntimeClass(TEXT("ZCBokoblinEnemy")))
		UBlueprintEditorLibrary::ReparentBlueprint(BP, RuntimeClass(TEXT("ZCBokoblinEnemy")));
	UObject* CDO = BP->GeneratedClass->GetDefaultObject();
	Set(CDO, TEXT("BehaviorTree"), Tree->GetPathName());
	Set(CDO, TEXT("AttackMontages"), FString::Printf(TEXT("(%s)"), *Attack->GetPathName()));
	Set(CDO, TEXT("AIControllerClass"), TEXT("/Script/ZCase.ZCBokoblinAIController"));
	Set(CDO, TEXT("AutoPossessAI"), TEXT("PlacedInWorldOrSpawned"));
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	check(BP->Status != BS_Error);
	Save(BP);
	// Load after reparenting so placed actors use the compiled class and keep instance transforms.
	check(FEditorFileUtils::LoadMap(TEXT("/Game/_Game/Maps/TestLevel"), false, true));
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);
	FBox EnemyBounds(ForceInit);
	int32 EnemyCount = 0;
	for (TActorIterator<ACharacter> It(World, BP->GeneratedClass.Get()); It; ++It)
	{
		Set(*It, TEXT("AIControllerClass"), TEXT("/Script/ZCase.ZCBokoblinAIController"));
		Set(*It, TEXT("AutoPossessAI"), TEXT("PlacedInWorldOrSpawned"));
		Set(*It, TEXT("BehaviorTree"), Tree->GetPathName());
		Set(*It, TEXT("AttackMontages"), FString::Printf(TEXT("(%s)"), *Attack->GetPathName()));
		EnemyBounds += It->GetActorLocation();
		++EnemyCount;
	}
	check(EnemyCount > 0);
	ANavMeshBoundsVolume* NavVolume = nullptr;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) { NavVolume = *It; break; }
	if (!NavVolume)
	{
		NavVolume = World->SpawnActor<ANavMeshBoundsVolume>(EnemyBounds.GetCenter(), FRotator::ZeroRotator);
		check(NavVolume);
		NavVolume->SetActorLabel(TEXT("BokoblinDemo_NavMeshBounds"));
		auto* Builder = NewObject<UCubeBuilder>(NavVolume);
		const FVector Size = EnemyBounds.GetSize() + FVector(7000, 7000, 1600);
		Builder->X = Size.X;
		Builder->Y = Size.Y;
		Builder->Z = Size.Z;
		UActorFactory::CreateBrushForVolumeActor(NavVolume, Builder);
	}
	if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Nav->OnNavigationBoundsUpdated(NavVolume);
		ARecastNavMesh* Recast = nullptr;
		for (TActorIterator<ARecastNavMesh> It(World); It; ++It) { Recast = *It; break; }
		if (!Recast)
		{
			FActorSpawnParameters Spawn;
			Spawn.OverrideLevel = World->PersistentLevel;
			Recast = World->SpawnActor<ARecastNavMesh>(ARecastNavMesh::StaticClass(), FTransform::Identity, Spawn);
			check(Recast);
			Recast->SetActorLabel(TEXT("RecastNavMesh-Default"));
		}
		Recast->SetConfig(UNavigationSystemV1::GetDefaultSupportedAgent());
		Nav->RequestRegistrationDeferred(*Recast);
		Nav->Tick(0.0f);
		Nav->Build();
		for (TActorIterator<ANavigationData> It(World); It; ++It)
		{
			It->EnsureBuildCompletion();
			It->MarkPackageDirty();
			if (It->IsPackageExternal())
			{
				UPackage* ExternalPackage = It->GetExternalPackage();
				const FString Filename = FPackageName::LongPackageNameToFilename(
					ExternalPackage->GetName(), FPackageName::GetAssetPackageExtension());
				FSavePackageArgs Args;
				Args.TopLevelFlags = RF_Public | RF_Standalone;
				check(UPackage::SavePackage(ExternalPackage, nullptr, *Filename, Args));
			}
			UE_LOG(LogTemp, Display, TEXT("Bokoblin setup navigation data %s in %s"), *It->GetName(), *GetNameSafe(It->GetLevel()));
		}
	}
	check(FEditorFileUtils::SaveLevel(World->PersistentLevel));
	UE_LOG(LogTemp, Display, TEXT("Bokoblin setup configured %d placed enemies and navigation bounds"), EnemyCount);
	UE_LOG(LogTemp, Display, TEXT("BOKOBLIN_ASSETS_OK"));
	return 0;
}
