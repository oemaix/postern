# Import from the NixOS configuration.
{ ... }:
{
  services.udev.extraRules = builtins.readFile ./serial.rules;
}
