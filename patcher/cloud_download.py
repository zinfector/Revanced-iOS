"""Download an HTTPS IPA without logging its possibly secret URL."""
import hashlib
from pathlib import Path
import re
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


class DownloadError(RuntimeError):
    pass


def validate_url(url):
    try:
        parts = urlsplit(url)
        valid = parts.scheme == 'https' and parts.hostname and not parts.username and not parts.password
        valid = valid and not parts.fragment and not any(ord(c) < 33 for c in url)
        _ = parts.port
    except (TypeError, ValueError):
        valid = False
    if not valid:
        raise DownloadError('IPA URL must be HTTPS without credentials, whitespace or a fragment')
    return url


class HTTPSRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        validate_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, output, expected_sha256, maximum=1024**3):
    validate_url(url)
    if not re.fullmatch(r'[a-fA-F0-9]{64}', expected_sha256 or ''):
        raise DownloadError('An exact 64-character source IPA SHA-256 is required')
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    owned = False
    try:
        request = Request(url, headers={'User-Agent': 'RVPort-cloud-sign/0.3'})
        with build_opener(HTTPSRedirects()).open(request, timeout=60) as response:
            if response.status != 200:
                raise DownloadError('IPA download did not return HTTP 200')
            length = response.headers.get('Content-Length')
            if length and (int(length) < 1 or int(length) > maximum):
                raise DownloadError('IPA download exceeds the size limit')
            digest = hashlib.sha256()
            count = 0
            with output.open('xb') as stream:
                owned = True
                while chunk := response.read(1024 * 1024):
                    count += len(chunk)
                    if count > maximum:
                        raise DownloadError('IPA download exceeds the size limit')
                    digest.update(chunk)
                    stream.write(chunk)
            if digest.hexdigest() != expected_sha256.lower():
                raise DownloadError('Downloaded IPA SHA-256 does not match; signing refused')
            return digest.hexdigest()
    except Exception as error:
        if owned:
            output.unlink(missing_ok=True)
        if isinstance(error, DownloadError):
            raise
        if isinstance(error, FileExistsError):
            raise DownloadError('Download output already exists') from None
        raise DownloadError('IPA download failed; check the URL, access and TLS connection') from None
