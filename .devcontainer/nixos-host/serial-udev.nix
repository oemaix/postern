# Import from the NixOS configuration (P-0033).
{ ... }:
{
  services.udev.extraRules = builtins.readFile ./serial.rules;
}
