using System.ComponentModel;
using System.Runtime.CompilerServices;

public class MeshPartItem : INotifyPropertyChanged
{
    private bool _isChecked;

    public string Name { get; set; } = "";
    public MeshPart MeshPart { get; set; } = new();

    public bool IsChecked
    {
        get => _isChecked;
        set
        {
            if (_isChecked == value)
                return;

            _isChecked = value;
            OnPropertyChanged();
        }
    }

    public string DisplayText => $"{Name}  (V:{MeshPart.VertexCount}, I:{MeshPart.IndexCount})";

    public event PropertyChangedEventHandler? PropertyChanged;

    protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}