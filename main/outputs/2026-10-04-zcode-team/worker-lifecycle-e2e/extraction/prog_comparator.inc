            if (mod_name != elem->get_name())
            {
                found = false;
            }
            if (is_installed == IsInstalled::Yes && !elem->is_installable())
            {
                found = false;
            }
