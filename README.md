drm_hwcomposer
======

Patches to drm_hwcomposer are very much welcome, we really want this to be the
universal HW composer implementation for Android and similar platforms
So please bring on porting patches, bugfixes, improvements for documentation
and new features.

A short list of contribution guidelines:
* Submit changes via gitlab merge requests on gitlab.freedesktop.org
* drm_hwcomposer is Apache 2.0 Licensed and we require contributions to follow the developer's certificate of origin: http://developercertificate.org/
* When submitting new code please follow the naming conventions documented in the generated documentation. Also please make full use of all the helpers and convenience macros provided by drm_hwcomposer. The below command can help you with formatting of your patches:
  
      `git diff | clang-format-diff-5.0 -p 1 -style=file`
* Hardware specific changes should be tested on relevant platforms before committing.

If you need inspiration, please checkout our [TODO issues](https://gitlab.freedesktop.org/drm-hwcomposer/drm-hwcomposer/issues?label_name%5B%5D=TODO)

Happy hacking!
