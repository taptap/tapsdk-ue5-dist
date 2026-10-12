#!/usr/bin/env python3
import hashlib
import json
import os
import time
import uuid
from urllib import error, parse, request

METHOD = "POST"
MAX_RETRIES = 10

def require_env(name: str) -> str:
    value = os.environ.get(name, "").strip()
    if not value:
        raise ValueError(f"{name} is required")
    return value


def parse_int_env(name: str) -> int:
    value = require_env(name)
    try:
        return int(value)
    except ValueError as exc:
        raise ValueError(f"{name} must be an integer, got {value!r}") from exc


def parse_extra_env(name: str) -> dict[str, str]:
    raw = os.environ.get(name, "").strip() or "{}"
    try:
        value = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"{name} must be a JSON object string") from exc

    if not isinstance(value, dict):
        raise ValueError(f"{name} must be a JSON object string")

    extra: dict[str, str] = {}
    for key, item in value.items():
        if not isinstance(key, str) or not isinstance(item, str):
            raise ValueError(f"{name} must be a map<string,string>")
        extra[key] = item
    return extra


def resolve_branch() -> str:
    return os.environ.get("GITHUB_HEAD_REF") or os.environ.get("GITHUB_REF_NAME") or ""


def resolve_history_type() -> str:
    history_type = os.environ.get("INPUT_HISTORY_TYPE", "demo").strip() or "demo"
    if history_type not in ("demo", "sdk"):
        raise ValueError(f"INPUT_HISTORY_TYPE must be demo or sdk, got {history_type!r}")
    return history_type


def resolve_base_url() -> str:
    base_url = require_env("HISTORY_HOST")
    return f"{base_url}/tapsdk/release"


def resolve_runtime_config() -> dict[str, object]:
    crypto_secret = require_env("HISTORY_CRYPTO_SECRET")
    history_type = resolve_history_type()
    base_url = resolve_base_url()
    if history_type == "sdk":
        target = "/v1/save-sdk"
        payload = {
            "uuid": uuid.uuid4().hex,
            "trigger": require_env("INPUT_TRIGGER"),
            "sdk_type": parse_int_env("INPUT_SDK_TYPE"),
            "sdk_ver": require_env("INPUT_SDK_VER"),
            "is_public": parse_int_env("INPUT_IS_PUBLIC"),
            "branch": resolve_branch(),
            "commit_sha": os.environ.get("GITHUB_SHA", ""),
            "url": require_env("INPUT_URL"),
            "size": parse_int_env("INPUT_SIZE"),
        }
    else:
        target = "/v1/save-demo"
        payload = {
            "uuid": uuid.uuid4().hex,
            "trigger": require_env("INPUT_TRIGGER"),
            "framework": parse_int_env("INPUT_FRAMEWORK"),
            "platform": parse_int_env("INPUT_PLATFORM"),
            "build_type": parse_int_env("INPUT_BUILD_TYPE"),
            "branch": resolve_branch(),
            "commit_sha": os.environ.get("GITHUB_SHA", ""),
            "extra": parse_extra_env("INPUT_EXTRA"),
            "url": require_env("INPUT_URL"),
            "size": parse_int_env("INPUT_SIZE"),
        }
    return {
        "history_type": history_type,
        "name": f"save-{history_type}",
        "url": build_signed_url(base_url, target, crypto_secret),
        "payload": payload,
    }


def build_sign(params: list[tuple[str, str]], secret: str) -> str:
    grouped: dict[str, list[str]] = {}
    for key, value in params:
        grouped.setdefault(key, []).append(value)

    parts = []
    for key in sorted(grouped):
        for value in sorted(grouped[key]):
            parts.append(f"{key}={value}")
    raw = "&".join(parts) + secret
    return hashlib.md5(raw.encode("utf-8")).hexdigest()


def build_signed_url(base_url: str, path: str, secret: str) -> str:
    query = [
        ("time", str(int(time.time()))),
        ("nonce", uuid.uuid4().hex[:12]),
    ]
    query.append(("sign", build_sign(query, secret)))
    return f"{base_url.rstrip('/')}{path}?{parse.urlencode(query)}"


class NoRedirectHandler(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def send_request(config: dict[str, object], body: str) -> str:
    opener = request.build_opener(NoRedirectHandler)

    for retry_index in range(MAX_RETRIES + 1):
        if retry_index > 0:
            delay_seconds = (retry_index - 1) * 5
            if delay_seconds > 0:
                print(f"network error, retrying in {delay_seconds}s (attempt {retry_index + 1}/{MAX_RETRIES + 1})")
                time.sleep(delay_seconds)
            else:
                print(f"network error, retrying immediately (attempt {retry_index + 1}/{MAX_RETRIES + 1})")

        req = request.Request(
            config["url"],
            data=body.encode("utf-8"),
            headers={
                "accept": "application/json",
                "content-type": "application/json",
            },
            method=METHOD,
        )

        try:
            with opener.open(req, timeout=10) as response:
                response_text = response.read().decode("utf-8")
                if response.status >= 300:
                    print(f"request failed with status: {response.status}")
                    print(response_text)
                    raise RuntimeError(f"unexpected http status {response.status}")
                return response_text
        except error.HTTPError as exc:
            print(f"request failed with status: {exc.code}")
            print(exc.read().decode("utf-8"))
            raise
        except error.URLError as exc:
            if retry_index == MAX_RETRIES:
                print(f"request failed after {MAX_RETRIES} retries: {exc}")
                raise
            print(f"request network error: {exc}")

    raise RuntimeError("request retries exhausted")


def handle_response(config: dict[str, object], response_text: str) -> int:
    response_json = json.loads(response_text)
    history_type = config["history_type"]

    if response_json.get("success") is not True:
        print("request failed: response.success is not true")
        return 1

    data = response_json.get("data", {})
    if not isinstance(data, dict):
        print("request failed: response.data is not an object")
        return 1

    if history_type == "sdk":
        return 0

    result_url = data.get("url", "")
    if not isinstance(result_url, str) or not result_url:
        print("request failed: response.data.url is missing")
        return 1

    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as fp:
            fp.write(f"url={result_url}\n")
    return 0


def main() -> int:
    config = resolve_runtime_config()
    body = json.dumps(config["payload"], ensure_ascii=False, separators=(",", ":"))

    print(f"{config['name']} payload:")
    print(json.dumps(config["payload"], ensure_ascii=False, separators=(",", ":")))
    print("request url:")
    print(config["url"])
    print("request body:")
    print(body)

    try:
        response_text = send_request(config, body)
        print(response_text)
        return handle_response(config, response_text)
    except error.HTTPError:
        return 1
    except error.URLError as exc:
        print(f"request failed: {exc}")
        return 1
    except RuntimeError as exc:
        print(f"request failed: {exc}")
        return 1
    except ValueError as exc:
        print(f"request failed: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
