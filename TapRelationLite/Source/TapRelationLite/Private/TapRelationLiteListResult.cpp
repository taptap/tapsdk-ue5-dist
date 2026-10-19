// Copyright TapTap. All Rights Reserved.
#include "TapRelationLiteListResult.h"

#include "TapRelationLiteCallbackUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace TapRelationLiteListResult
{
    FParsed Parse(const FString& Json, const FString& ListKey)
    {
        FParsed Out;
        TSharedPtr<FJsonObject> Root;
        const auto Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Out;

        // Native failures contain only error_message. Even an empty message is a failure.
        if (Root->HasField(TEXT("error_message")))
        {
            if (!Root->TryGetStringField(TEXT("error_message"), Out.ErrorMessage) || Out.ErrorMessage.IsEmpty())
                Out.ErrorMessage = TEXT("Unknown RelationLite error");
            return Out;
        }

        FTapRelationLiteUserResult Result;
        FString ListJson;
        if (!Root->TryGetStringField(TEXT("next_page_token"), Result.NextPageToken) ||
            !Root->TryGetStringField(ListKey, ListJson)) return Out;

        TArray<TSharedPtr<FJsonValue>> Items;
        const auto ListReader = TJsonReaderFactory<>::Create(ListJson);
        if (!FJsonSerializer::Deserialize(ListReader, Items, FJsonSerializer::EFlags::StoreNumbersAsStrings)) return Out;
        for (const auto& Value : Items)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) return Out;
            const auto Object = Value->AsObject();
            const TSharedPtr<FJsonObject>* User = nullptr;
            const TSharedPtr<FJsonObject>* Status = nullptr;
            FTapRelationLiteUserItem Item;
            if (!Object.IsValid() ||
                !Object->TryGetNumberField(TEXT("created_at"), Item.CreatedAt) ||
                !Object->TryGetObjectField(TEXT("user"), User) || !User || !User->IsValid() ||
                !Object->TryGetObjectField(TEXT("follow_status"), Status) || !Status || !Status->IsValid()) return Out;

            (*User)->TryGetStringField(TEXT("alias"), Item.User.Alias);
            (*User)->TryGetStringField(TEXT("avatar"), Item.User.Avatar);
            (*User)->TryGetStringField(TEXT("name"), Item.User.Name);
            (*User)->TryGetStringField(TEXT("open_id"), Item.User.OpenId);
            (*Status)->TryGetBoolField(TEXT("blocked_by"), Item.FollowStatus.bBlockedBy);
            (*Status)->TryGetBoolField(TEXT("blocking"), Item.FollowStatus.bBlocking);
            (*Status)->TryGetBoolField(TEXT("followed_by"), Item.FollowStatus.bFollowedBy);
            (*Status)->TryGetBoolField(TEXT("following"), Item.FollowStatus.bFollowing);
            Result.Users.Add(MoveTemp(Item));
        }

        Out.bSuccess = true;
        Out.Result = MoveTemp(Result);
        Out.ErrorMessage.Reset();
        return Out;
    }

    void Complete(const FString& Json, const FString& ListKey,
        TFunction<void(const FTapRelationLiteUserResult&)> OnSuccess,
        TFunction<void(int32, const FString&)> OnFailure, TFunction<bool()> IsCurrent)
    {
        FParsed Parsed = Parse(Json, ListKey);
        TapRelationLiteCallbackUtils::Dispatch(
            [Parsed = MoveTemp(Parsed), OnSuccess = MoveTemp(OnSuccess), OnFailure = MoveTemp(OnFailure)]()
            {
                if (Parsed.bSuccess)
                {
                    if (OnSuccess) OnSuccess(Parsed.Result);
                }
                else
                {
                    // The native bridge supplies no error code; match Unity's -1 fallback.
                    if (OnFailure) OnFailure(-1, Parsed.ErrorMessage);
                }
            }, MoveTemp(IsCurrent));
    }
}
