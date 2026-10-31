# Spec Delta

## REMOVED Requirements

### Requirement: Game sources stay out of this link
**Reason**: This change links every game and overlay translation unit into `hmbtn-host`, which is the point of the host build. The window and OpenGL presentation keep working with those units on the link.
**Migration**: None for users. The host target now lists the `psx_exe` and `psx_overlay` sources beside the PC port sources; the window, clear, and close behavior are unchanged.
