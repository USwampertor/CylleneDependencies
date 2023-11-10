# Copyright 2019 NVIDIA CORPORATION

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Use these for throwing exceptions that should be reported as errors to the user (when using command line version)
# or handled by the programmer (when using packmanapi)


class PackmanError(Exception):
    pass


class PackmanErrorFileExists(PackmanError):
    pass


class PackmanErrorFileNotFound(PackmanError):
    pass


class PackmanErrorFileNotLink(PackmanError):
    pass


class PackmanErrorScriptFailure(PackmanError):
    def __init__(self, script_name: str, error_code: int):
        PackmanError.__init__(self, "Script '%s' returned error code %d" % (script_name, error_code))
        self.error_code = error_code


class PackmanErrorProgressIncomplete(PackmanError):
    pass


class PackmanErrorSignatureInvalid(PackmanError):
    pass
